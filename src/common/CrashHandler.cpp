#include "CrashHandler.h"

#ifdef _WIN32

#include <windows.h>
#include <dbghelp.h>
#include <string>
#include <fstream>
#include <chrono>
#include <ctime>
#include <filesystem>

#pragma comment(lib, "dbghelp.lib")

static std::string getCrashDir() {
    char appData[MAX_PATH] = {0};
    GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
    std::filesystem::path dir = std::string(appData) + "\\HtkisPro\\crash";
    std::filesystem::create_directories(dir);
    return dir.string();
}

static std::string getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", std::localtime(&t));
    char result[80];
    snprintf(result, sizeof(result), "%s_%03lld", buf, (long long)ms.count());
    return result;
}

static LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* ep) {
    std::string dir = getCrashDir();
    std::string timestamp = getTimestamp();
    std::string dumpPath = dir + "\\htkispro_" + timestamp + ".dmp";

    HANDLE hFile = CreateFileA(dumpPath.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei;
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;

        BOOL ok = MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            hFile,
            MiniDumpNormal,
            &mei,
            nullptr,
            nullptr
        );
        CloseHandle(hFile);

        if (ok) {
            std::ofstream logFile(dir + "\\crash_log.txt", std::ios::app);
            if (logFile.is_open()) {
                logFile << "[" << timestamp << "] Minidump written to " << dumpPath
                        << " ExceptionCode=0x" << std::hex << ep->ExceptionRecord->ExceptionCode
                        << " Address=0x" << ep->ExceptionRecord->ExceptionAddress << std::endl;
            }
            return EXCEPTION_EXECUTE_HANDLER;
        }
    }

    std::ofstream fallback(dir + "\\crash_fallback_" + timestamp + ".txt");
    if (fallback.is_open()) {
        fallback << "MiniDumpWriteDump failed." << std::endl;
        fallback << "ExceptionCode: 0x" << std::hex << ep->ExceptionRecord->ExceptionCode << std::endl;
        fallback << "ExceptionAddress: 0x" << ep->ExceptionRecord->ExceptionAddress << std::endl;
        fallback << "ExceptionFlags: 0x" << ep->ExceptionRecord->ExceptionFlags << std::endl;
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

void CrashHandler::init() {
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
}

#endif