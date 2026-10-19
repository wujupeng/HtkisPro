#pragma once

#ifdef _WIN32

#include <string>

class CrashHandler {
public:
    static CrashHandler& getInstance() {
        static CrashHandler instance;
        return instance;
    }

    void init();

private:
    CrashHandler() = default;
};

#endif