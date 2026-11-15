#pragma once

#include <stddef.h>

typedef void (*CrashHandlerCallback)(char*, size_t*);

#ifdef __cplusplus
extern "C" {
#endif

void CrashHandlerRegisterCallback(CrashHandlerCallback callback);
void CrashHandlerSetNote(const char* key, const char* value);
int CrashMonitorRunIfRequested(int argc, char** argv, int* exitCode);

#ifdef __cplusplus
}
#endif
