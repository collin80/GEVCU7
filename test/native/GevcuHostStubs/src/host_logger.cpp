/*
 * Logger replacement that records every message instead of writing to
 * serial/SD so tests can assert on errors the firmware reports.
 */
#include "Logger.h"
#include "host_test_support.h"

uint32_t Logger::lastLogTime = 0;
ESP32Driver *Logger::esp32 = nullptr;
static Logger::LogLevel hostLogLevel = Logger::Info;
static std::vector<HostLogRecord> records;

static void record(int level, uint16_t device, const char *fmt, va_list args)
{
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, args);
    records.push_back({level, device, buf});
}

#define HOST_LOG_FN(name, lvl)                                                         \
    void Logger::name(const char *fmt, ...)                                            \
    { va_list a; va_start(a, fmt); record(lvl, 0xFFFF, fmt, a); va_end(a); }           \
    void Logger::name(DeviceId id, const char *fmt, ...)                               \
    { va_list a; va_start(a, fmt); record(lvl, id, fmt, a); va_end(a); }

HOST_LOG_FN(avalanche, 0)
HOST_LOG_FN(debug, 1)
HOST_LOG_FN(info, 2)
HOST_LOG_FN(warn, 3)
HOST_LOG_FN(error, 4)

void Logger::console(const char *fmt, ...)
{
    va_list a;
    va_start(a, fmt);
    record(5, 0xFFFF, fmt, a);
    va_end(a);
}

void Logger::setLoglevel(LogLevel level) { hostLogLevel = level; }
Logger::LogLevel Logger::getLogLevel() { return hostLogLevel; }
uint32_t Logger::getLastLogTime() { return lastLogTime; }
boolean Logger::isDebug() { return hostLogLevel == Debug; }
void Logger::initializeFile() {}
void Logger::loop() {}
void Logger::flushFile() {}

const std::vector<HostLogRecord> &hostLogRecords() { return records; }
void hostLogClear() { records.clear(); }

int hostLogCount(int level)
{
    int n = 0;
    for (auto &r : records) if (r.level == level) n++;
    return n;
}

bool hostLogContains(int level, const char *substring)
{
    for (auto &r : records)
        if (r.level == level && r.text.find(substring) != std::string::npos) return true;
    return false;
}
