#include "log.h"

#include <Windows.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

namespace {
    std::mutex g_logMutex;
    std::wstring g_logPath;
}

void LogSetPath(const wchar_t* path) {
    std::lock_guard lock(g_logMutex);
    g_logPath = path ? path : L"";
}

void LogEvent(const char* format, ...) {
    char message[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    char line[560];
    std::snprintf(line, sizeof(line), "[VarjoXRLayersSupport] %s\n", message);
    OutputDebugStringA(line);

    std::lock_guard lock(g_logMutex);

    if (g_logPath.empty()) 
        return;

    FILE* file = nullptr;

    if (_wfopen_s(&file, g_logPath.c_str(), L"a") == 0 && file) {
        std::fputs(line, file);
        std::fclose(file);
    }
}
