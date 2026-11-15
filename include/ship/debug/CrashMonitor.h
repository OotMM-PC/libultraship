#pragma once

#include <cstddef>
#include <string>

struct _EXCEPTION_POINTERS;

namespace Ship {
namespace CrashMonitor {
typedef void (*GameInfoCallback)(char*, size_t*);

constexpr unsigned long kPureCallCode = 0xE0000001;
constexpr unsigned long kInvalidParameterCode = 0xE0000002;
constexpr unsigned long kAbortCode = 0x40000015;
constexpr unsigned long kCppExceptionCode = 0xE06D7363;

// Runs the crash monitor when the command line asks for it; the caller returns exitCode straight away.
bool RunIfRequested(int argc, char** argv, int* exitCode);
bool Start(const std::string& product, const std::string& shortName, const std::string& reportDirectory);
void SetNote(const char* key, const char* value);
void AppendLog(const char* text, size_t length);
// Hands the crash to the monitor and ends the process; returns only when no monitor can take it.
bool Report(_EXCEPTION_POINTERS* exception, const char* kind, const char* detail, GameInfoCallback callback);
bool ReportHere(unsigned long code, const char* kind, const char* detail, GameInfoCallback callback);
void SelfTestTick();
} // namespace CrashMonitor
} // namespace Ship
