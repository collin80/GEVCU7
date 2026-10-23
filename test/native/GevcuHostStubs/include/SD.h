/*
 * Host stand-in for the Teensy SD/SdFat headers. FsFile reads lines from an
 * in-memory string so file-driven code (e.g. intel_hex) can be exercised.
 */
#pragma once

#include <Arduino.h>

class FsFile : public Stream
{
public:
    FsFile() {}
    explicit FsFile(const char *contents) : data(contents) {}
    int fgets(char *line, int maxbytes)
    {
        if (maxbytes <= 0) return -1;
        if (pos >= data.size()) { line[0] = 0; return 0; }
        int n = 0;
        while (n < maxbytes - 1 && pos < data.size())
        {
            char c = data[pos++];
            line[n++] = c;
            if (c == '\n') break;
        }
        line[n] = 0;
        return n;
    }
    int available() override { return (int)(data.size() - pos); }
    int read() override { return pos < data.size() ? (uint8_t)data[pos++] : -1; }
    explicit operator bool() const { return true; }
    void close() {}

private:
    std::string data;
    size_t pos = 0;
};
