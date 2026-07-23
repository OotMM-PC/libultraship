#include "libultraship/bridge/IpcClient.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace Ship {

namespace {

constexpr uint32_t MaxFrameSize = 4 * 1024 * 1024;
constexpr size_t MaxQueuedFrames = 1024;

std::vector<uint8_t> EncodeFrame(const std::string& message) {
    const auto size = static_cast<uint32_t>(message.size());
    std::vector<uint8_t> frame(4 + message.size());
    frame[0] = static_cast<uint8_t>(size);
    frame[1] = static_cast<uint8_t>(size >> 8);
    frame[2] = static_cast<uint8_t>(size >> 16);
    frame[3] = static_cast<uint8_t>(size >> 24);
    std::copy(message.begin(), message.end(), frame.begin() + 4);
    return frame;
}

#ifdef _WIN32
std::wstring ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                                           nullptr, 0);
    if (length <= 0) {
        return {};
    }

    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(),
                        length);
    return result;
}
#endif

} // namespace

class IpcClient::Impl {
  public:
    bool Start(const std::string& endpoint) {
        Stop();
        if (endpoint.empty()) {
            SetError("IPC endpoint is empty");
            return false;
        }

#ifndef _WIN32
        SetError("IPC is not supported on this platform");
        return false;
#else
        mEndpoint = ToWide(endpoint);
        if (mEndpoint.empty()) {
            SetError("IPC endpoint is not valid UTF-8");
            return false;
        }

        mStopping = false;
        mWorker = std::thread(&Impl::Run, this);
        return true;
#endif
    }

    void Stop() {
        mStopping = true;
        mWake.notify_all();
        if (mWorker.joinable()) {
            mWorker.join();
        }

        mConnected = false;
        ClearQueues();
    }

    bool Send(std::string message) {
        if (!mConnected || message.size() > MaxFrameSize) {
            return false;
        }

        {
            std::lock_guard lock(mOutboundMutex);
            if (mOutbound.size() >= MaxQueuedFrames) {
                SetError("IPC outbound queue is full");
                return false;
            }
            mOutbound.emplace_back(std::move(message));
        }
        mWake.notify_one();
        return true;
    }

    bool TryReceive(std::string& message) {
        std::lock_guard lock(mInboundMutex);
        if (mInbound.empty()) {
            return false;
        }

        message = std::move(mInbound.front());
        mInbound.pop_front();
        return true;
    }

    bool IsConnected() const {
        return mConnected;
    }

    std::string GetLastError() const {
        std::lock_guard lock(mErrorMutex);
        return mLastError;
    }

  private:
#ifdef _WIN32
    void Run() {
        while (!mStopping) {
            if (!WaitNamedPipeW(mEndpoint.c_str(), 200)) {
                const auto error = ::GetLastError();
                if (error != ERROR_FILE_NOT_FOUND && error != ERROR_SEM_TIMEOUT && error != ERROR_PIPE_BUSY) {
                    SetWindowsError("WaitNamedPipe failed", error);
                }
                std::unique_lock lock(mWaitMutex);
                mWake.wait_for(lock, std::chrono::milliseconds(50), [this] { return mStopping.load(); });
                continue;
            }

            HANDLE pipe = CreateFileW(mEndpoint.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0,
                                      nullptr);
            if (pipe == INVALID_HANDLE_VALUE) {
                SetWindowsError("CreateFile failed", ::GetLastError());
                continue;
            }

            DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
            if (!SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr)) {
                SetWindowsError("SetNamedPipeHandleState failed", ::GetLastError());
                CloseHandle(pipe);
                continue;
            }

            SetError({});
            mConnected = true;
            RunConnected(pipe);
            mConnected = false;
            CloseHandle(pipe);
            ClearOutbound();

            if (!mStopping) {
                std::unique_lock lock(mWaitMutex);
                mWake.wait_for(lock, std::chrono::milliseconds(100), [this] { return mStopping.load(); });
            }
        }
    }

    void RunConnected(HANDLE pipe) {
        std::vector<uint8_t> incoming;
        std::vector<uint8_t> pending;
        size_t pendingOffset = 0;

        while (!mStopping) {
            bool didWork = false;
            if (pending.empty()) {
                std::string message;
                {
                    std::lock_guard lock(mOutboundMutex);
                    if (!mOutbound.empty()) {
                        message = std::move(mOutbound.front());
                        mOutbound.pop_front();
                    }
                }
                if (!message.empty()) {
                    pending = EncodeFrame(message);
                    pendingOffset = 0;
                }
            }

            if (!pending.empty()) {
                DWORD written = 0;
                const auto remaining = static_cast<DWORD>(pending.size() - pendingOffset);
                if (WriteFile(pipe, pending.data() + pendingOffset, remaining, &written, nullptr)) {
                    pendingOffset += written;
                    didWork = written > 0;
                    if (pendingOffset == pending.size()) {
                        pending.clear();
                    }
                } else {
                    const auto error = ::GetLastError();
                    if (error != ERROR_NO_DATA && error != ERROR_PIPE_BUSY) {
                        SetWindowsError("IPC write failed", error);
                        return;
                    }
                }
            }

            std::array<uint8_t, 4096> buffer{};
            DWORD read = 0;
            if (ReadFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
                if (read > 0) {
                    incoming.insert(incoming.end(), buffer.begin(), buffer.begin() + read);
                    didWork = true;
                    if (!ParseFrames(incoming)) {
                        return;
                    }
                }
            } else {
                const auto error = ::GetLastError();
                if (error != ERROR_NO_DATA) {
                    SetWindowsError("IPC read failed", error);
                    return;
                }
            }

            if (!didWork) {
                std::unique_lock lock(mWaitMutex);
                mWake.wait_for(lock, std::chrono::milliseconds(5), [this] {
                    return mStopping.load() || HasOutbound();
                });
            }
        }
    }

    bool ParseFrames(std::vector<uint8_t>& buffer) {
        size_t offset = 0;
        while (buffer.size() - offset >= 4) {
            const uint32_t size = static_cast<uint32_t>(buffer[offset]) |
                                  static_cast<uint32_t>(buffer[offset + 1]) << 8 |
                                  static_cast<uint32_t>(buffer[offset + 2]) << 16 |
                                  static_cast<uint32_t>(buffer[offset + 3]) << 24;
            if (size > MaxFrameSize) {
                SetError("IPC frame exceeds the size limit");
                return false;
            }
            if (buffer.size() - offset - 4 < size) {
                break;
            }

            {
                std::lock_guard lock(mInboundMutex);
                if (mInbound.size() >= MaxQueuedFrames) {
                    SetError("IPC inbound queue is full");
                    return false;
                }
                mInbound.emplace_back(reinterpret_cast<const char*>(buffer.data() + offset + 4), size);
            }
            offset += 4 + size;
        }

        if (offset > 0) {
            buffer.erase(buffer.begin(), buffer.begin() + offset);
        }
        return true;
    }

    void SetWindowsError(const char* operation, DWORD error) {
        SetError(std::string(operation) + " (" + std::to_string(error) + ")");
    }
#endif

    bool HasOutbound() {
        std::lock_guard lock(mOutboundMutex);
        return !mOutbound.empty();
    }

    void ClearQueues() {
        {
            std::lock_guard lock(mInboundMutex);
            mInbound.clear();
        }
        ClearOutbound();
    }

    void ClearOutbound() {
        std::lock_guard lock(mOutboundMutex);
        mOutbound.clear();
    }

    void SetError(std::string error) {
        std::lock_guard lock(mErrorMutex);
        mLastError = std::move(error);
    }

    std::atomic<bool> mStopping = false;
    std::atomic<bool> mConnected = false;
    std::thread mWorker;
    std::condition_variable mWake;
    std::mutex mWaitMutex;
    std::deque<std::string> mInbound;
    std::mutex mInboundMutex;
    std::deque<std::string> mOutbound;
    std::mutex mOutboundMutex;
    std::string mLastError;
    mutable std::mutex mErrorMutex;
#ifdef _WIN32
    std::wstring mEndpoint;
#endif
};

IpcClient::IpcClient() : mImpl(std::make_unique<Impl>()) {
}

IpcClient::~IpcClient() {
    Stop();
}

bool IpcClient::Start(const std::string& endpoint) {
    return mImpl->Start(endpoint);
}

void IpcClient::Stop() {
    mImpl->Stop();
}

bool IpcClient::Send(std::string message) {
    return mImpl->Send(std::move(message));
}

bool IpcClient::TryReceive(std::string& message) {
    return mImpl->TryReceive(message);
}

bool IpcClient::IsConnected() const {
    return mImpl->IsConnected();
}

std::string IpcClient::GetLastError() const {
    return mImpl->GetLastError();
}

} // namespace Ship
