#pragma once
#include <string>
struct ILogSink {
    virtual ~ILogSink() = default;
    virtual void info(const std::string& m) = 0;
    virtual void error(const std::string& m) = 0;
};

struct NullLogSink : ILogSink {
    void info(const std::string&) override {}
    void error(const std::string&) override {}
};

namespace Log {
    ILogSink* SetSink(ILogSink* s);   // devuelve el anterior
    void Info(const std::string& m);
    void Error(const std::string& m);
}
