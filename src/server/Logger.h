#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <ctime>
#include <deque>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <optional>

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

struct LogEntry {
    std::time_t time{};
    LogLevel level{LogLevel::Info};
    std::string message;
    std::string module;
    uint64_t sessionId{0};
    std::thread::id threadId{};
    uint64_t version{0};
};

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void debug(const std::string& msg, const std::string& module = "", uint64_t sessionId = 0) {
        add(LogLevel::Debug, msg, module, sessionId);
    }
    void info(const std::string& msg, const std::string& module = "", uint64_t sessionId = 0) {
        add(LogLevel::Info, msg, module, sessionId);
    }
    void warn(const std::string& msg, const std::string& module = "", uint64_t sessionId = 0) {
        add(LogLevel::Warn, msg, module, sessionId);
    }
    void error(const std::string& msg, const std::string& module = "", uint64_t sessionId = 0) {
        add(LogLevel::Error, msg, module, sessionId);
    }

    void setMinLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_minLevel = level;
    }

    LogLevel getMinLevel() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_minLevel;
    }

    std::vector<LogEntry> getEntries() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::vector<LogEntry>(m_entries.begin(), m_entries.end());
    }

    std::pair<std::vector<LogEntry>, uint64_t> getEntriesSince(uint64_t version) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<LogEntry> result;
        for (const auto& entry : m_entries) {
            if (entry.version > version) {
                result.push_back(entry);
            }
        }
        return {result, m_nextVersion - 1};
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.clear();
        m_dirty = false;
    }

    bool hasNewEntries() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_dirty;
    }

    void markRead() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dirty = false;
    }

    void enableFileSink(const std::string& path, LogLevel minLevel = LogLevel::Info) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_fileThreadRunning) return;

        m_fileStream = std::make_unique<std::ofstream>(path, std::ios::app);
        if (!m_fileStream->is_open()) return;

        m_fileMinLevel = minLevel;
        m_fileThreadRunning = true;
        m_fileThread = std::thread([this]() { fileSinkLoop(); });
    }

    void disableFileSink() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_fileThreadRunning) return;
            m_fileThreadRunning = false;
        }
        m_fileCond.notify_all();
        if (m_fileThread.joinable()) m_fileThread.join();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fileStream.reset();
    }

    static const size_t MAX_ENTRIES = 2000;

private:
    Logger() = default;

    ~Logger() {
        disableFileSink();
    }

    void add(LogLevel level, const std::string& msg, const std::string& module, uint64_t sessionId) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (level < m_minLevel) return;

            LogEntry entry;
            entry.time = std::time(nullptr);
            entry.level = level;
            entry.message = msg;
            entry.module = module;
            entry.sessionId = sessionId;
            entry.threadId = std::this_thread::get_id();
            entry.version = m_nextVersion++;

            m_entries.push_back(entry);
            if (m_entries.size() > MAX_ENTRIES) {
                m_entries.erase(m_entries.begin(), m_entries.begin() + (m_entries.size() - MAX_ENTRIES));
            }
            m_dirty = true;

            if (m_fileThreadRunning && level >= m_fileMinLevel) {
                m_fileCond.notify_one();
            }
        }
    }

    void fileSinkLoop() {
        while (true) {
            std::vector<LogEntry> toWrite;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_fileCond.wait(lock, [this]() {
                    return !m_fileThreadRunning || hasPendingFileWrites();
                });
                if (!m_fileThreadRunning) break;

                for (const auto& entry : m_entries) {
                    if (entry.version > m_fileWrittenVersion && entry.level >= m_fileMinLevel) {
                        toWrite.push_back(entry);
                        m_fileWrittenVersion = entry.version;
                    }
                }
            }

            if (m_fileStream && m_fileStream->is_open()) {
                for (const auto& entry : toWrite) {
                    char timeBuf[64];
                    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", std::localtime(&entry.time));
                    (*m_fileStream) << timeBuf << " [" << levelToString(entry.level) << "]";
                    if (!entry.module.empty()) (*m_fileStream) << " [" << entry.module << "]";
                    if (entry.sessionId > 0) (*m_fileStream) << " [sid=" << entry.sessionId << "]";
                    (*m_fileStream) << " " << entry.message << "\n";
                }
                m_fileStream->flush();
            }
        }
    }

    bool hasPendingFileWrites() const {
        return m_nextVersion > m_fileWrittenVersion + 1;
    }

    static const char* levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info:  return "INFO";
            case LogLevel::Warn:  return "WARN";
            case LogLevel::Error: return "ERROR";
            default: return "?";
        }
    }

    std::deque<LogEntry> m_entries;
    mutable std::mutex m_mutex;
    bool m_dirty = false;
    LogLevel m_minLevel = LogLevel::Info;
    uint64_t m_nextVersion = 1;

    std::unique_ptr<std::ofstream> m_fileStream;
    std::thread m_fileThread;
    std::atomic<bool> m_fileThreadRunning{false};
    std::condition_variable m_fileCond;
    LogLevel m_fileMinLevel = LogLevel::Info;
    uint64_t m_fileWrittenVersion = 0;
};
