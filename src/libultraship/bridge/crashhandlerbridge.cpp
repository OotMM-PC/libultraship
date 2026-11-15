#include "libultraship/bridge/crashhandlerbridge.h"
#include "ship/Context.h"
#include "ship/debug/CrashHandler.h"
#include "ship/debug/CrashMonitor.h"

void CrashHandlerRegisterCallback(CrashHandlerCallback callback) {
    Ship::Context::GetInstance()->GetCrashHandler()->RegisterCallback(callback);
}

void CrashHandlerSetNote(const char* key, const char* value) {
    Ship::CrashMonitor::SetNote(key, value);
}

int CrashMonitorRunIfRequested(int argc, char** argv, int* exitCode) {
    return Ship::CrashMonitor::RunIfRequested(argc, argv, exitCode) ? 1 : 0;
}
