#include "ship/debug/CrashMonitor.h"

#if defined(_WIN32) && defined(_M_AMD64)

#include <windows.h>
#include <DbgHelp.h>
#include <intrin.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "Psapi.lib")

extern "C" int __cdecl _purecall(void);

namespace Ship {
namespace CrashMonitor {
namespace {

constexpr uint32_t kMagic = 0x52434D4F;
constexpr size_t kNoteCount = 16;
constexpr size_t kNoteKeySize = 32;
constexpr size_t kNoteValueSize = 480;
constexpr size_t kLogSize = 128 * 1024;
constexpr size_t kGameInfoSize = 64 * 1024;
constexpr DWORD kGameInfoWaitMs = 2000;
constexpr DWORD kMonitorWaitMs = 60000;
constexpr DWORD kStackGuaranteeBytes = 64 * 1024;
constexpr size_t kKeptReports = 20;
constexpr int kMaxFrames = 96;
constexpr char kMonitorArgument[] = "--ootmm-crash-monitor";
constexpr char kSelfTestVariable[] = "OOTMM_CRASH_TEST";
constexpr DWORD kHeapCorruptionCode = 0xC0000374;

struct Note {
    char Key[kNoteKeySize];
    char Value[kNoteValueSize];
};

struct Shared {
    uint32_t Magic;
    uint32_t Size;
    volatile LONG Phase;
    DWORD ProcessId;
    DWORD ThreadId;
    uint32_t HasContext;
    uint64_t ExceptionPointers;
    EXCEPTION_RECORD Record;
    CONTEXT Context;
    char Kind[64];
    char Detail[512];
    char Product[64];
    char ShortName[32];
    wchar_t Directory[1024];
    Note Notes[kNoteCount];
    volatile LONG64 LogWritten;
    char Log[kLogSize];
    size_t GameInfoLength;
    char GameInfo[kGameInfoSize];
};

enum Phase : LONG { kRunning = 0, kCrashed = 1, kGameInfoDone = 2 };

Shared sLocal;
Shared* sBlock = &sLocal;
Shared* sMapped = nullptr;
SRWLOCK sWriteLock = SRWLOCK_INIT;
HANDLE sCrashEvent = nullptr;
HANDLE sDoneEvent = nullptr;
HANDLE sMonitorProcess = nullptr;
volatile LONG sClaimed = 0;
volatile DWORD sCrashThread = 0;
std::atomic<int> sMainThreadTest{ 0 };

void CopyText(char* out, size_t size, const char* text) {
    size_t i = 0;
    if (text != nullptr) {
        for (; i + 1 < size && text[i] != '\0'; i++) {
            out[i] = text[i];
        }
    }
    out[i] = '\0';
}

std::wstring ObjectName(DWORD pid, const wchar_t* suffix) {
    wchar_t name[96];
    swprintf_s(name, L"Local\\OotmmCrash-%lu%s", pid, suffix);
    return name;
}

bool MonitorAlive() {
    return sMapped != nullptr && sMonitorProcess != nullptr &&
           WaitForSingleObject(sMonitorProcess, 0) == WAIT_TIMEOUT;
}

void RunGameInfo(GameInfoCallback callback) {
    __try {
        callback(sMapped->GameInfo, &sMapped->GameInfoLength);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

struct CppThrowInfo {
    uint32_t Attributes;
    int32_t Unwind;
    int32_t ForwardCompat;
    int32_t CatchableTypeArray;
};

struct CppCatchableType {
    uint32_t Properties;
    int32_t Type;
    int32_t MemberDisplacement;
    int32_t VbtableDisplacement;
    int32_t VbtableOffset;
    int32_t Size;
    int32_t CopyFunction;
};

struct CppTypeDescriptor {
    const void* Vftable;
    void* Spare;
    char Name[1];
};

void PrettyTypeName(const char* decorated, char* out, size_t size) {
    const char* start = decorated;
    if (strncmp(start, ".?A", 3) == 0 && start[3] != '\0') {
        start += 4;
    }
    const char* parts[8];
    size_t lengths[8];
    int count = 0;
    while (*start != '\0' && *start != '@' && count < 8) {
        const char* end = strchr(start, '@');
        if (end == nullptr) {
            end = start + strlen(start);
        }
        parts[count] = start;
        lengths[count++] = static_cast<size_t>(end - start);
        start = *end == '@' ? end + 1 : end;
    }
    size_t used = 0;
    for (int i = count - 1; i >= 0; i--) {
        for (size_t c = 0; c < lengths[i] && used + 1 < size; c++) {
            out[used++] = parts[i][c];
        }
        if (i > 0 && used + 2 < size) {
            out[used++] = ':';
            out[used++] = ':';
        }
    }
    out[used] = '\0';
}

void DescribeCppException(const EXCEPTION_RECORD* record, char* out, size_t size) {
    out[0] = '\0';
    if (record->ExceptionCode != kCppExceptionCode || record->NumberParameters < 4) {
        return;
    }
    __try {
        const auto* object = reinterpret_cast<const char*>(record->ExceptionInformation[1]);
        const auto* info = reinterpret_cast<const CppThrowInfo*>(record->ExceptionInformation[2]);
        const auto* base = reinterpret_cast<const char*>(record->ExceptionInformation[3]);
        if (object == nullptr || info == nullptr || base == nullptr) {
            return;
        }
        const auto* types = reinterpret_cast<const int32_t*>(base + info->CatchableTypeArray);
        const int32_t count = types[0];
        char typeName[160] = "";
        const char* message = nullptr;
        for (int32_t i = 0; i < count && i < 16; i++) {
            const auto* type = reinterpret_cast<const CppCatchableType*>(base + types[1 + i]);
            const auto* descriptor = reinterpret_cast<const CppTypeDescriptor*>(base + type->Type);
            if (i == 0) {
                PrettyTypeName(descriptor->Name, typeName, sizeof(typeName));
            }
            if (strcmp(descriptor->Name, ".?AVexception@std@@") == 0 && type->VbtableDisplacement == -1) {
                message = reinterpret_cast<const std::exception*>(object + type->MemberDisplacement)->what();
            }
        }
        if (message != nullptr) {
            _snprintf_s(out, size, _TRUNCATE, "%s: %s", typeName, message);
        } else {
            _snprintf_s(out, size, _TRUNCATE, "%s", typeName);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

#pragma warning(push)
#pragma warning(disable : 4717)
__declspec(noinline) void OverflowStack(volatile char* previous) {
    volatile char page[4096];
    page[0] = previous != nullptr ? previous[0] : 0;
    OverflowStack(page);
    page[1] = page[0];
}
#pragma warning(pop)

enum SelfTest { kTestNone, kTestAccess, kTestHeap, kTestAbort, kTestThrow, kTestPureCall, kTestStack, kTestFastFail };

SelfTest ParseSelfTest(const std::string& kind) {
    static const struct {
        const char* Name;
        SelfTest Test;
    } kTests[] = { { "av", kTestAccess },         { "heap", kTestHeap },   { "abort", kTestAbort },
                   { "throw", kTestThrow },       { "purecall", kTestPureCall }, { "stack", kTestStack },
                   { "fastfail", kTestFastFail } };
    for (const auto& test : kTests) {
        if (kind == test.Name) {
            return test.Test;
        }
    }
    return kTestNone;
}

void RunSelfTest(SelfTest test) {
    switch (test) {
        case kTestAccess:
            *reinterpret_cast<volatile int*>(static_cast<uintptr_t>(0x10)) = 1;
            break;
        case kTestHeap: {
            auto* block = static_cast<char*>(HeapAlloc(GetProcessHeap(), 0, 64));
            memset(block - 16, 0xAA, 16);
            HeapFree(GetProcessHeap(), 0, block);
            break;
        }
        case kTestAbort:
            abort();
        case kTestThrow:
            throw std::runtime_error("crash self-test");
        case kTestPureCall:
            _purecall();
            break;
        case kTestStack:
            OverflowStack(nullptr);
            break;
        case kTestFastFail:
            __fastfail(FAST_FAIL_FATAL_APP_EXIT);
        default:
            break;
    }
}

void StartSelfTest() {
    const char* spec = std::getenv(kSelfTestVariable);
    if (spec == nullptr || spec[0] == '\0') {
        return;
    }
    std::string text = spec;
    const bool onMainThread = text.size() > 5 && text.compare(text.size() - 5, 5, "@main") == 0;
    if (onMainThread) {
        text.resize(text.size() - 5);
    }
    unsigned long seconds = 8;
    if (const size_t colon = text.find(':'); colon != std::string::npos) {
        seconds = strtoul(text.c_str() + colon + 1, nullptr, 10);
        text.resize(colon);
    }
    const SelfTest test = ParseSelfTest(text);
    if (test == kTestNone) {
        return;
    }
    std::thread([test, seconds, onMainThread]() {
        Sleep(seconds * 1000);
        if (onMainThread) {
            sMainThreadTest.store(test);
        } else {
            RunSelfTest(test);
        }
    }).detach();
}

std::string Format(const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
    va_end(args);
    return buffer;
}

std::string Narrow(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr,
                                         nullptr);
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring Widen(const std::string& narrow) {
    if (narrow.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, narrow.data(), static_cast<int>(narrow.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, narrow.data(), static_cast<int>(narrow.size()), out.data(), size);
    return out;
}

const char* ExceptionName(DWORD code) {
    switch (code) {
        case 0xC0000005:
            return "Access violation";
        case 0xC0000006:
            return "In-page I/O error";
        case 0xC0000017:
        case 0xC000012D:
            return "Out of memory";
        case 0xC000001D:
            return "Illegal instruction";
        case 0xC0000025:
            return "Non-continuable exception";
        case 0xC000008C:
            return "Array bounds exceeded";
        case 0xC0000094:
            return "Integer divide by zero";
        case 0xC0000096:
            return "Privileged instruction";
        case 0xC00000FD:
            return "Stack overflow";
        case 0xC0000135:
            return "DLL not found";
        case 0xC0000139:
            return "DLL entry point not found";
        case 0xC0000142:
            return "DLL initialization failed";
        case 0xC0000374:
            return "Heap corruption";
        case 0xC0000409:
            return "Fast fail (stack buffer overrun or fatal check)";
        case 0xC0000417:
            return "Invalid C runtime parameter";
        case 0xC000041D:
            return "Exception inside a Windows callback";
        case 0xC0000420:
            return "Assertion failure";
        case 0x80000003:
            return "Breakpoint";
        case kAbortCode:
            return "abort()";
        case kCppExceptionCode:
            return "Unhandled C++ exception";
        case kPureCallCode:
            return "Pure virtual call";
        case kInvalidParameterCode:
            return "Invalid C runtime parameter";
        default:
            return "Fatal exception";
    }
}

bool IsFatalExit(DWORD code) {
    return (code & 0xF0000000) == 0xC0000000 || (code & 0xF0000000) == 0xE0000000 || code == 0x80000003 ||
           code == kAbortCode;
}

struct Module {
    DWORD64 Base;
    DWORD Size;
    std::wstring Path;
    std::string Name;
};

struct Frame {
    DWORD64 Address;
    std::string Symbol;
    std::string Line;
};

struct ThreadStack {
    DWORD Id;
    std::string Name;
    bool Crashed;
    CONTEXT Context;
    bool HasContext;
    std::vector<Frame> Frames;
};

struct Target {
    HANDLE Process = nullptr;
    DWORD ProcessId = 0;
    const Shared* Block = nullptr;
    std::vector<Module> Modules;
    std::vector<std::pair<DWORD, HANDLE>> Threads;
    std::vector<ThreadStack> Stacks;
    bool SymbolsReady = false;
    std::wstring ReportPath;
    std::string DumpStatus;
    std::string WindowsRecord;
    DWORD ExitCode = 0;
    bool Vanished = false;
};

void LoadModules(Target& target) {
    HMODULE handles[1024];
    DWORD needed = 0;
    if (!EnumProcessModulesEx(target.Process, handles, sizeof(handles), &needed, LIST_MODULES_ALL)) {
        return;
    }
    const size_t count = (std::min)(static_cast<size_t>(needed / sizeof(HMODULE)), std::size(handles));
    for (size_t i = 0; i < count; i++) {
        MODULEINFO info{};
        wchar_t path[1024] = L"";
        if (!GetModuleInformation(target.Process, handles[i], &info, sizeof(info)) ||
            GetModuleFileNameExW(target.Process, handles[i], path, static_cast<DWORD>(std::size(path))) == 0) {
            continue;
        }
        const wchar_t* name = wcsrchr(path, L'\\');
        target.Modules.push_back({ reinterpret_cast<DWORD64>(info.lpBaseOfDll), info.SizeOfImage, path,
                                   Narrow(name != nullptr ? name + 1 : path) });
    }
    std::sort(target.Modules.begin(), target.Modules.end(),
              [](const Module& a, const Module& b) { return a.Base < b.Base; });
}

std::string FormatAddress(const Target& target, DWORD64 address) {
    for (const Module& module : target.Modules) {
        if (address >= module.Base && address < module.Base + module.Size) {
            return Format("%s+0x%llX", module.Name.c_str(), address - module.Base);
        }
    }
    return Format("0x%016llX", address);
}

void SuspendThreads(Target& target) {
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    for (BOOL more = Thread32First(snapshot, &entry); more; more = Thread32Next(snapshot, &entry)) {
        if (entry.th32OwnerProcessID != target.ProcessId) {
            continue;
        }
        const HANDLE thread = OpenThread(THREAD_ALL_ACCESS, FALSE, entry.th32ThreadID);
        if (thread == nullptr) {
            continue;
        }
        SuspendThread(thread);
        target.Threads.emplace_back(entry.th32ThreadID, thread);
    }
    CloseHandle(snapshot);
}

void ResumeThreads(Target& target) {
    for (const auto& [id, thread] : target.Threads) {
        ResumeThread(thread);
        CloseHandle(thread);
    }
    target.Threads.clear();
}

std::string ThreadName(HANDLE thread) {
    using GetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PWSTR*);
    static const auto getDescription = reinterpret_cast<GetThreadDescriptionFn>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetThreadDescription"));
    PWSTR description = nullptr;
    std::string name;
    if (getDescription != nullptr && SUCCEEDED(getDescription(thread, &description)) && description != nullptr) {
        name = Narrow(description);
        LocalFree(description);
    }
    return name;
}

void WalkThread(Target& target, HANDLE thread, ThreadStack& stack) {
    CONTEXT context = stack.Context;
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    for (int i = 0; i < kMaxFrames; i++) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, target.Process, thread, &frame, &context, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr) ||
            frame.AddrPC.Offset == 0) {
            break;
        }
        Frame entry{ frame.AddrPC.Offset, {}, {} };
        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        if (SymFromAddr(target.Process, frame.AddrPC.Offset, &displacement, symbol)) {
            entry.Symbol = Format("%s+0x%llX", symbol->Name, displacement);
        }
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddr64(target.Process, frame.AddrPC.Offset, &lineDisplacement, &line)) {
            entry.Line = Format("%s:%lu", line.FileName, line.LineNumber);
        }
        stack.Frames.push_back(std::move(entry));
    }
}

void CollectStacks(Target& target) {
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS |
                  SYMOPT_NO_PROMPTS);
    std::wstring searchPath;
    if (!target.Modules.empty()) {
        for (const Module& module : target.Modules) {
            if (module.Name.size() > 4 && _stricmp(module.Name.c_str() + module.Name.size() - 4, ".exe") == 0) {
                const std::wstring directory = module.Path.substr(0, module.Path.find_last_of(L'\\'));
                searchPath = directory + L";" + directory + L"\\debug";
                break;
            }
        }
    }
    if (!SymInitializeW(target.Process, searchPath.empty() ? nullptr : searchPath.c_str(), TRUE)) {
        return;
    }
    target.SymbolsReady = true;
    for (const auto& [id, thread] : target.Threads) {
        ThreadStack stack{};
        stack.Id = id;
        stack.Name = ThreadName(thread);
        stack.Crashed = target.Block->HasContext != 0 && id == target.Block->ThreadId;
        if (stack.Crashed) {
            stack.Context = target.Block->Context;
            stack.HasContext = true;
        } else {
            stack.Context.ContextFlags = CONTEXT_FULL;
            stack.HasContext = GetThreadContext(thread, &stack.Context) != 0;
        }
        if (stack.HasContext) {
            WalkThread(target, thread, stack);
        }
        target.Stacks.push_back(std::move(stack));
    }
}

bool CollectStacksGuarded(Target& target) {
    __try {
        CollectStacks(target);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

BOOL WriteDumpGuarded(HANDLE process, DWORD pid, HANDLE file, MINIDUMP_EXCEPTION_INFORMATION* exception) {
    __try {
        const auto type = static_cast<MINIDUMP_TYPE>(
            MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo |
            MiniDumpWithUnloadedModules | MiniDumpWithProcessThreadData | MiniDumpIgnoreInaccessibleMemory);
        return MiniDumpWriteDump(process, pid, file, type, exception, nullptr, nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_UNHANDLED_EXCEPTION);
        return FALSE;
    }
}

void WriteDump(Target& target, const std::wstring& path) {
    const HANDLE file =
        CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        target.DumpStatus = Format("not written (cannot create file, error %lu)", GetLastError());
        return;
    }
    MINIDUMP_EXCEPTION_INFORMATION exception{};
    exception.ThreadId = target.Block->ThreadId;
    exception.ExceptionPointers = reinterpret_cast<PEXCEPTION_POINTERS>(target.Block->ExceptionPointers);
    exception.ClientPointers = TRUE;
    const BOOL written = WriteDumpGuarded(target.Process, target.ProcessId, file,
                                          target.Block->ExceptionPointers != 0 ? &exception : nullptr);
    const DWORD error = GetLastError();
    LARGE_INTEGER size{};
    GetFileSizeEx(file, &size);
    CloseHandle(file);
    if (!written) {
        DeleteFileW(path.c_str());
        target.DumpStatus = Format("not written (error 0x%08lX)", error);
        return;
    }
    const wchar_t* name = wcsrchr(path.c_str(), L'\\');
    target.DumpStatus = Format("%s (%.1f MB)", Narrow(name != nullptr ? name + 1 : path.c_str()).c_str(),
                               static_cast<double>(size.QuadPart) / (1024.0 * 1024.0));
}

std::string RecentLog(const Shared& block) {
    const LONG64 written = block.LogWritten;
    std::string log;
    if (written <= static_cast<LONG64>(kLogSize)) {
        log.assign(block.Log, static_cast<size_t>(written));
        return log;
    }
    const size_t head = static_cast<size_t>(written % static_cast<LONG64>(kLogSize));
    log.assign(block.Log + head, kLogSize - head);
    log.append(block.Log, head);
    const size_t firstLine = log.find('\n');
    return firstLine == std::string::npos ? log : log.substr(firstLine + 1);
}

std::string DescribeException(const Target& target) {
    const EXCEPTION_RECORD& record = target.Block->Record;
    std::string text = Format("0x%08lX %s", record.ExceptionCode, ExceptionName(record.ExceptionCode));
    if ((record.ExceptionCode == 0xC0000005 || record.ExceptionCode == 0xC0000006) &&
        record.NumberParameters >= 2) {
        const ULONG_PTR mode = record.ExceptionInformation[0];
        text += Format(" %s address 0x%016llX", mode == 0 ? "reading" : mode == 1 ? "writing" : "executing",
                       static_cast<unsigned long long>(record.ExceptionInformation[1]));
    }
    return text;
}

std::string SystemLine(const Target& target) {
    OSVERSIONINFOEXW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOEXW*);
    if (const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll")) {
        if (const auto getVersion = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"))) {
            getVersion(&version);
        }
    }
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    GlobalMemoryStatusEx(&memory);
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    GetProcessMemoryInfo(target.Process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters));
    const double gb = 1024.0 * 1024.0 * 1024.0;
    return Format("Windows %lu.%lu.%lu, RAM %.1f GB (%.1f GB free), commit %.1f of %.1f GB, game memory %.0f MB "
                  "(peak %.0f MB)",
                  version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber, memory.ullTotalPhys / gb,
                  memory.ullAvailPhys / gb, (memory.ullTotalPageFile - memory.ullAvailPageFile) / gb,
                  memory.ullTotalPageFile / gb, counters.PrivateUsage / (1024.0 * 1024.0),
                  counters.PeakPagefileUsage / (1024.0 * 1024.0));
}

double RunSeconds(const Target& target) {
    FILETIME created{}, exited{}, kernel{}, user{}, now{};
    if (!GetProcessTimes(target.Process, &created, &exited, &kernel, &user)) {
        return 0.0;
    }
    GetSystemTimeAsFileTime(&now);
    const ULONGLONG start = (static_cast<ULONGLONG>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
    const ULONGLONG end = (static_cast<ULONGLONG>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
    return end > start ? static_cast<double>(end - start) / 10000000.0 : 0.0;
}

bool IsRuntimeFrame(const Frame& frame) {
    static const char* const kPrefixes[] = { "minkernel\\", "D:\\a\\_work\\" };
    static const char* const kFragments[] = { "\\VC\\Tools\\MSVC\\", "\\vctools\\", "CrashHandler.cpp" };
    for (const char* prefix : kPrefixes) {
        if (frame.Line.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    for (const char* fragment : kFragments) {
        if (frame.Line.find(fragment) != std::string::npos) {
            return true;
        }
    }
    return frame.Symbol.find("CrashMonitor::Report") != std::string::npos;
}

std::string Culprit(const Target& target) {
    for (const ThreadStack& stack : target.Stacks) {
        if (!stack.Crashed) {
            continue;
        }
        for (const Frame& frame : stack.Frames) {
            if (frame.Line.empty() || frame.Symbol.empty() || IsRuntimeFrame(frame)) {
                continue;
            }
            const size_t slash = frame.Line.find_last_of("\\/");
            return frame.Symbol.substr(0, frame.Symbol.rfind('+')) + " (" +
                   (slash == std::string::npos ? frame.Line : frame.Line.substr(slash + 1)) + ")";
        }
    }
    return {};
}

std::string FirstGameFrame(const Target& target) {
    for (const ThreadStack& stack : target.Stacks) {
        if (!stack.Crashed) {
            continue;
        }
        for (const Frame& frame : stack.Frames) {
            for (const Module& module : target.Modules) {
                if (frame.Address < module.Base || frame.Address >= module.Base + module.Size) {
                    continue;
                }
                const size_t length = module.Name.size();
                if (length > 4 && _stricmp(module.Name.c_str() + length - 4, ".exe") == 0) {
                    return Format("%s+0x%llX", module.Name.c_str(), frame.Address - module.Base);
                }
                break;
            }
        }
    }
    return {};
}

std::string Summary(const Target& target) {
    const Shared& block = *target.Block;
    if (target.Vanished) {
        return Format("%s ended without a crash signal, exit code 0x%08lX (%s)", block.Product, target.ExitCode,
                      ExceptionName(target.ExitCode));
    }
    std::string summary = Format("%s (0x%08lX)", block.Kind, block.Record.ExceptionCode);
    if (const std::string culprit = Culprit(target); !culprit.empty()) {
        summary += " in " + culprit;
    } else if (block.HasContext != 0) {
        const std::string address = FormatAddress(target, block.Context.Rip);
        summary += " at " + address;
        if (const std::string game = FirstGameFrame(target); !game.empty() && game != address) {
            summary += ", reached from " + game;
        }
    }
    if (block.Detail[0] != '\0' && block.Record.ExceptionCode != kHeapCorruptionCode) {
        summary += std::string(": ") + block.Detail;
    }
    return summary;
}

void AppendStack(std::string& out, const Target& target, const ThreadStack& stack) {
    if (!stack.HasContext) {
        out += "  (no thread context)\n";
        return;
    }
    if (stack.Frames.empty()) {
        out += Format("  #00 %s\n", FormatAddress(target, stack.Context.Rip).c_str());
        return;
    }
    for (size_t i = 0; i < stack.Frames.size(); i++) {
        const Frame& frame = stack.Frames[i];
        out += Format("  #%02zu %-34s", i, FormatAddress(target, frame.Address).c_str());
        if (!frame.Symbol.empty()) {
            out += " " + frame.Symbol;
        }
        if (!frame.Line.empty()) {
            out += "  (" + frame.Line + ")";
        }
        out += "\n";
    }
}

void AppendRegisters(std::string& out, const CONTEXT& c) {
    out += Format("  RAX %016llX  RBX %016llX  RCX %016llX  RDX %016llX\n", c.Rax, c.Rbx, c.Rcx, c.Rdx);
    out += Format("  RSI %016llX  RDI %016llX  RBP %016llX  RSP %016llX\n", c.Rsi, c.Rdi, c.Rbp, c.Rsp);
    out += Format("  R8  %016llX  R9  %016llX  R10 %016llX  R11 %016llX\n", c.R8, c.R9, c.R10, c.R11);
    out += Format("  R12 %016llX  R13 %016llX  R14 %016llX  R15 %016llX\n", c.R12, c.R13, c.R14, c.R15);
    out += Format("  RIP %016llX  EFLAGS %08lX\n", c.Rip, c.EFlags);
}

void AppendModules(std::string& out, const Target& target) {
    for (const Module& module : target.Modules) {
        std::string symbols;
        IMAGEHLP_MODULEW64 info{};
        info.SizeOfStruct = sizeof(info);
        if (target.SymbolsReady && SymGetModuleInfoW64(target.Process, module.Base, &info)) {
            const GUID& g = info.PdbSig70;
            symbols = Format("  time %08lX  pdb %08lX%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X-%lX%s", info.TimeDateStamp,
                             g.Data1, g.Data2, g.Data3, g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4],
                             g.Data4[5], g.Data4[6], g.Data4[7], info.PdbAge,
                             info.SymType == SymPdb ? "  symbols loaded" : "");
        }
        out += Format("  %016llX  %8lX  %s%s\n", module.Base, module.Size, Narrow(module.Path).c_str(),
                      symbols.c_str());
    }
}

std::string Compose(const Target& target, bool withStacks) {
    const Shared& block = *target.Block;
    SYSTEMTIME utc{}, local{};
    GetSystemTime(&utc);
    GetLocalTime(&local);
    std::string out = Format("%s crash report\n", block.Product);
    out += "Summary: " + Summary(target) + "\n";
    out += Format("Time: %04u-%02u-%02u %02u:%02u:%02u UTC (local %02u:%02u:%02u)\n", utc.wYear, utc.wMonth, utc.wDay,
                  utc.wHour, utc.wMinute, utc.wSecond, local.wHour, local.wMinute, local.wSecond);
    out += Format("Process: pid %lu, ran for %.1f seconds\n", target.ProcessId, RunSeconds(target));
    out += "System: " + SystemLine(target) + "\n";
    if (!target.Vanished) {
        out += "Memory dump: " + target.DumpStatus + "\n";
    }

    out += "\nWhat happened\n";
    if (target.Vanished) {
        out += Format("  The game process ended with exit code 0x%08lX (%s) and never signalled a crash.\n",
                      target.ExitCode, ExceptionName(target.ExitCode));
        out += "  This kind of failure cannot be caught inside the game, so there is no stack or memory dump.\n";
    } else {
        out += Format("  Kind: %s\n", block.Kind);
        out += "  Exception: " + DescribeException(target) + "\n";
        if (block.Detail[0] != '\0') {
            out += Format("  Detail: %s\n", block.Detail);
        }
        if (block.HasContext != 0) {
            out += "  Address: " + FormatAddress(target, block.Context.Rip) + "\n";
        }
        out += Format("  Thread: %lu\n", block.ThreadId);
    }

    out += "\nNotes\n";
    bool anyNote = false;
    for (const Note& note : block.Notes) {
        if (note.Key[0] != '\0') {
            out += Format("  %.*s: %.*s\n", static_cast<int>(kNoteKeySize), note.Key, static_cast<int>(kNoteValueSize),
                          note.Value);
            anyNote = true;
        }
    }
    if (!anyNote) {
        out += "  (none)\n";
    }

    if (!target.WindowsRecord.empty()) {
        out += "\nWindows crash record\n" + target.WindowsRecord;
    }

    if (!target.Vanished) {
        out += "\nCrashing thread stack\n";
        const ThreadStack* crashed = nullptr;
        for (const ThreadStack& stack : target.Stacks) {
            if (stack.Crashed) {
                crashed = &stack;
            }
        }
        if (crashed != nullptr) {
            AppendStack(out, target, *crashed);
        } else if (withStacks) {
            out += "  (stack walk unavailable; open the memory dump)\n";
        } else {
            out += "  (pending)\n";
        }
        if (block.HasContext != 0) {
            out += "\nRegisters\n";
            AppendRegisters(out, block.Context);
        }
    }

    out += "\nGame state\n";
    const size_t gameInfoLength = (std::min)(block.GameInfoLength, kGameInfoSize);
    if (gameInfoLength > 0) {
        out.append(block.GameInfo, gameInfoLength);
        if (out.back() != '\n') {
            out += "\n";
        }
    } else {
        out += "  (not available)\n";
    }

    if (!target.Vanished && withStacks) {
        out += "\nOther threads\n";
        for (const ThreadStack& stack : target.Stacks) {
            if (stack.Crashed) {
                continue;
            }
            out += Format("Thread %lu%s%s\n", stack.Id, stack.Name.empty() ? "" : " ", stack.Name.c_str());
            AppendStack(out, target, stack);
        }
    }

    if (!target.Modules.empty()) {
        out += "\nModules\n";
        AppendModules(out, target);
    }

    out += "\nRecent log\n";
    out += RecentLog(block);
    if (out.back() != '\n') {
        out += "\n";
    }
    return out;
}

void WriteText(const std::wstring& path, const std::string& text) {
    const HANDLE file =
        CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    size_t offset = 0;
    while (offset < text.size()) {
        DWORD written = 0;
        const DWORD chunk = static_cast<DWORD>((std::min)(text.size() - offset, static_cast<size_t>(1 << 20)));
        if (!WriteFile(file, text.data() + offset, chunk, &written, nullptr) || written == 0) {
            break;
        }
        offset += written;
    }
    FlushFileBuffers(file);
    CloseHandle(file);
}

std::string RunCapture(std::wstring command) {
    SECURITY_ATTRIBUTES attributes{ sizeof(attributes), nullptr, TRUE };
    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &attributes, 0)) {
        return {};
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    PROCESS_INFORMATION process{};
    std::string output;
    if (CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                       &process)) {
        CloseHandle(writePipe);
        writePipe = nullptr;
        char buffer[4096];
        DWORD read = 0;
        while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0 && output.size() < (1 << 20)) {
            output.append(buffer, read);
        }
        WaitForSingleObject(process.hProcess, 10000);
        CloseHandle(process.hProcess);
        CloseHandle(process.hThread);
    }
    if (writePipe != nullptr) {
        CloseHandle(writePipe);
    }
    CloseHandle(readPipe);
    return output;
}

std::string EventField(const std::string& event, const char* name) {
    const std::string open = std::string("<Data Name='") + name + "'>";
    const size_t start = event.find(open);
    if (start == std::string::npos) {
        return {};
    }
    const size_t end = event.find("</Data>", start);
    return end == std::string::npos ? std::string() : event.substr(start + open.size(), end - start - open.size());
}

std::string WindowsCrashRecord(DWORD pid) {
    for (int attempt = 0; attempt < 4; attempt++) {
        Sleep(1500);
        const std::string xml = RunCapture(
            L"wevtutil.exe qe Application \"/q:*[System[(EventID=1000)]]\" /c:12 /rd:true /f:xml");
        size_t position = 0;
        while ((position = xml.find("<Event ", position)) != std::string::npos) {
            const size_t end = xml.find("</Event>", position);
            const std::string event = xml.substr(position, end == std::string::npos ? std::string::npos : end - position);
            position += 7;
            if (strtoul(EventField(event, "ProcessId").c_str(), nullptr, 16) != pid) {
                continue;
            }
            return Format("  Faulting module: %s\n  Exception code: 0x%s\n  Fault offset: 0x%s\n  Module path: %s\n",
                          EventField(event, "ModuleName").c_str(), EventField(event, "ExceptionCode").c_str(),
                          EventField(event, "FaultingOffset").c_str(), EventField(event, "ModulePath").c_str());
        }
    }
    return {};
}

std::wstring ReportStem(const Shared& block, DWORD pid) {
    SYSTEMTIME local{};
    GetLocalTime(&local);
    wchar_t name[160];
    swprintf_s(name, L"%hs-%04u%02u%02u-%02u%02u%02u-%lu", block.ShortName, local.wYear, local.wMonth, local.wDay,
               local.wHour, local.wMinute, local.wSecond, pid);
    return std::wstring(block.Directory) + L"\\" + name;
}

void PruneReports(const std::wstring& directory) {
    struct Report {
        FILETIME Written;
        std::wstring Stem;
    };
    std::vector<Report> reports;
    WIN32_FIND_DATAW found{};
    const HANDLE search = FindFirstFileW((directory + L"\\*.txt").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        std::wstring name = found.cFileName;
        reports.push_back({ found.ftLastWriteTime, name.substr(0, name.size() - 4) });
    } while (FindNextFileW(search, &found));
    FindClose(search);
    std::sort(reports.begin(), reports.end(),
              [](const Report& a, const Report& b) { return CompareFileTime(&a.Written, &b.Written) > 0; });
    for (size_t i = kKeptReports; i < reports.size(); i++) {
        for (const wchar_t* extension : { L".txt", L".dmp", L".zip" }) {
            DeleteFileW((directory + L"\\" + reports[i].Stem + extension).c_str());
        }
    }
}

bool LaunchedStandalone() {
    return GetEnvironmentVariableW(L"OOTMM_BOOT_CONFIG", nullptr, 0) == 0 &&
           GetEnvironmentVariableW(L"OOTMM_RUNTIME_TEST_DIR", nullptr, 0) == 0 &&
           GetEnvironmentVariableW(L"OOTMM_PARENT_HWND", nullptr, 0) == 0;
}

void AnnounceReport(const Shared& block, const std::wstring& reportPath) {
    if (!LaunchedStandalone()) {
        return;
    }
    const std::wstring text = Widen(block.Product) + L" has crashed.\n\nA crash report was saved to:\n" + reportPath;
    MessageBoxW(nullptr, text.c_str(), L"Crash", MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST);
}

void HandleCrash(Target& target) {
    const Shared& block = *target.Block;
    for (DWORD waited = 0; block.Phase != kGameInfoDone && waited < kGameInfoWaitMs; waited += 25) {
        if (WaitForSingleObject(target.Process, 25) != WAIT_TIMEOUT) {
            break;
        }
    }
    CreateDirectoryW(block.Directory, nullptr);
    const std::wstring stem = ReportStem(block, target.ProcessId);
    SuspendThreads(target);
    LoadModules(target);
    target.DumpStatus = "pending";
    WriteText(stem + L".txt", Compose(target, false));
    WriteDump(target, stem + L".dmp");
    CollectStacksGuarded(target);
    WriteText(stem + L".txt", Compose(target, true));
    ResumeThreads(target);
    target.ReportPath = stem + L".txt";
}

void HandleExit(Target& target) {
    if (!GetExitCodeProcess(target.Process, &target.ExitCode) || !IsFatalExit(target.ExitCode)) {
        return;
    }
    const Shared& block = *target.Block;
    target.Vanished = true;
    CreateDirectoryW(block.Directory, nullptr);
    const std::wstring stem = ReportStem(block, target.ProcessId);
    WriteText(stem + L".txt", Compose(target, false));
    target.WindowsRecord = WindowsCrashRecord(target.ProcessId);
    WriteText(stem + L".txt", Compose(target, false));
    target.ReportPath = stem + L".txt";
}

int RunMonitor(DWORD pid) {
    const HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, ObjectName(pid, L"").c_str());
    if (mapping == nullptr) {
        return 2;
    }
    const auto* block = static_cast<const Shared*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(Shared)));
    if (block == nullptr || block->Magic != kMagic || block->Size != sizeof(Shared)) {
        return 2;
    }
    const HANDLE crashEvent = OpenEventW(SYNCHRONIZE, FALSE, ObjectName(pid, L"-crash").c_str());
    const HANDLE doneEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, ObjectName(pid, L"-done").c_str());
    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (process == nullptr) {
        process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE, FALSE, pid);
    }
    if (crashEvent == nullptr || doneEvent == nullptr || process == nullptr) {
        return 2;
    }
    PruneReports(block->Directory);

    Target target;
    target.Process = process;
    target.ProcessId = pid;
    target.Block = block;
    const HANDLE waits[2] = { crashEvent, process };
    if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) == WAIT_OBJECT_0) {
        HandleCrash(target);
        SetEvent(doneEvent);
        if (WaitForSingleObject(process, 3000) == WAIT_TIMEOUT) {
            TerminateProcess(process, block->Record.ExceptionCode);
        }
    } else {
        HandleExit(target);
    }
    if (!target.ReportPath.empty()) {
        AnnounceReport(*block, target.ReportPath);
    }
    return 0;
}

} // namespace

bool RunIfRequested(int argc, char** argv, int* exitCode) {
    for (int i = 1; i + 1 < argc; i++) {
        if (strcmp(argv[i], kMonitorArgument) == 0) {
            *exitCode = RunMonitor(strtoul(argv[i + 1], nullptr, 10));
            return true;
        }
    }
    return false;
}

bool Start(const std::string& product, const std::string& shortName, const std::string& reportDirectory) {
    if (sMapped != nullptr) {
        return true;
    }
    const DWORD pid = GetCurrentProcessId();
    const HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                              static_cast<DWORD>(sizeof(Shared)), ObjectName(pid, L"").c_str());
    if (mapping == nullptr) {
        return false;
    }
    auto* mapped = static_cast<Shared*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
    sCrashEvent = CreateEventW(nullptr, TRUE, FALSE, ObjectName(pid, L"-crash").c_str());
    sDoneEvent = CreateEventW(nullptr, TRUE, FALSE, ObjectName(pid, L"-done").c_str());
    if (mapped == nullptr || sCrashEvent == nullptr || sDoneEvent == nullptr) {
        return false;
    }

    wchar_t directory[1024] = L"";
    GetFullPathNameW(Widen(reportDirectory).c_str(), static_cast<DWORD>(std::size(directory)), directory, nullptr);
    AcquireSRWLockExclusive(&sWriteLock);
    memcpy(mapped, &sLocal, sizeof(Shared));
    mapped->Magic = kMagic;
    mapped->Size = sizeof(Shared);
    mapped->ProcessId = pid;
    CopyText(mapped->Product, sizeof(mapped->Product), product.c_str());
    CopyText(mapped->ShortName, sizeof(mapped->ShortName), shortName.c_str());
    wcscpy_s(mapped->Directory, directory);
    sBlock = mapped;
    ReleaseSRWLockExclusive(&sWriteLock);

    wchar_t executable[1024] = L"";
    GetModuleFileNameW(nullptr, executable, static_cast<DWORD>(std::size(executable)));
    wchar_t command[1200];
    swprintf_s(command, L"\"%s\" %hs %lu", executable, kMonitorArgument, pid);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable, command, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                        &process)) {
        return false;
    }
    CloseHandle(process.hThread);
    sMonitorProcess = process.hProcess;
    sMapped = mapped;

    ULONG guarantee = kStackGuaranteeBytes;
    SetThreadStackGuarantee(&guarantee);
    StartSelfTest();
    return true;
}

void SetNote(const char* key, const char* value) {
    if (key == nullptr || key[0] == '\0') {
        return;
    }
    AcquireSRWLockExclusive(&sWriteLock);
    for (Note& note : sBlock->Notes) {
        if (note.Key[0] == '\0' || strncmp(note.Key, key, kNoteKeySize - 1) == 0) {
            CopyText(note.Key, sizeof(note.Key), key);
            CopyText(note.Value, sizeof(note.Value), value);
            break;
        }
    }
    ReleaseSRWLockExclusive(&sWriteLock);
}

void AppendLog(const char* text, size_t length) {
    if (length > kLogSize) {
        text += length - kLogSize;
        length = kLogSize;
    }
    AcquireSRWLockExclusive(&sWriteLock);
    Shared* block = sBlock;
    const size_t head = static_cast<size_t>(block->LogWritten % static_cast<LONG64>(kLogSize));
    const size_t first = (std::min)(length, kLogSize - head);
    memcpy(block->Log + head, text, first);
    memcpy(block->Log, text + first, length - first);
    InterlockedAdd64(&block->LogWritten, static_cast<LONG64>(length));
    ReleaseSRWLockExclusive(&sWriteLock);
}

bool Report(_EXCEPTION_POINTERS* exception, const char* kind, const char* detail, GameInfoCallback callback) {
    if (exception == nullptr || !MonitorAlive()) {
        return false;
    }
    if (InterlockedCompareExchange(&sClaimed, 1, 0) != 0) {
        if (GetCurrentThreadId() == sCrashThread) {
            return false;
        }
        Sleep(INFINITE);
    }
    sCrashThread = GetCurrentThreadId();
    Shared* shared = sMapped;
    const DWORD code = exception->ExceptionRecord->ExceptionCode;
    shared->ThreadId = sCrashThread;
    shared->ExceptionPointers = reinterpret_cast<uint64_t>(exception);
    shared->Record = *exception->ExceptionRecord;
    shared->Context = *exception->ContextRecord;
    shared->HasContext = 1;
    CopyText(shared->Kind, sizeof(shared->Kind), kind);
    if (detail != nullptr && detail[0] != '\0') {
        CopyText(shared->Detail, sizeof(shared->Detail), detail);
    } else {
        DescribeCppException(exception->ExceptionRecord, shared->Detail, sizeof(shared->Detail));
    }
    InterlockedExchange(&shared->Phase, kCrashed);
    SetEvent(sCrashEvent);
    if (callback != nullptr && code != EXCEPTION_STACK_OVERFLOW) {
        RunGameInfo(callback);
    }
    InterlockedExchange(&shared->Phase, kGameInfoDone);
    WaitForSingleObject(sDoneEvent, kMonitorWaitMs);
    TerminateProcess(GetCurrentProcess(), code);
    Sleep(INFINITE);
    return true;
}

bool ReportHere(unsigned long code, const char* kind, const char* detail, GameInfoCallback callback) {
    static EXCEPTION_RECORD record;
    static CONTEXT context;
    static EXCEPTION_POINTERS pointers;
    if (!MonitorAlive() || sClaimed != 0) {
        return false;
    }
    RtlCaptureContext(&context);
    record = {};
    record.ExceptionCode = code;
    record.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
    record.ExceptionAddress = reinterpret_cast<PVOID>(context.Rip);
    pointers.ExceptionRecord = &record;
    pointers.ContextRecord = &context;
    return Report(&pointers, kind, detail, callback);
}

void SelfTestTick() {
    if (sMainThreadTest.load(std::memory_order_relaxed) != 0) {
        RunSelfTest(static_cast<SelfTest>(sMainThreadTest.exchange(0)));
    }
}

} // namespace CrashMonitor
} // namespace Ship

#else

namespace Ship {
namespace CrashMonitor {
bool RunIfRequested(int, char**, int*) {
    return false;
}

bool Start(const std::string&, const std::string&, const std::string&) {
    return false;
}

void SetNote(const char*, const char*) {
}

void AppendLog(const char*, size_t) {
}

bool Report(_EXCEPTION_POINTERS*, const char*, const char*, GameInfoCallback) {
    return false;
}

bool ReportHere(unsigned long, const char*, const char*, GameInfoCallback) {
    return false;
}

void SelfTestTick() {
}
} // namespace CrashMonitor
} // namespace Ship

#endif
