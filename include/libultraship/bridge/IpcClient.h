#pragma once

#include <memory>
#include <string>

namespace Ship {

class IpcClient {
  public:
    IpcClient();
    ~IpcClient();

    IpcClient(const IpcClient&) = delete;
    IpcClient& operator=(const IpcClient&) = delete;

    bool Start(const std::string& endpoint);
    void Stop();
    bool Send(std::string message);
    bool TryReceive(std::string& message);
    bool IsConnected() const;
    std::string GetLastError() const;

  private:
    class Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace Ship
