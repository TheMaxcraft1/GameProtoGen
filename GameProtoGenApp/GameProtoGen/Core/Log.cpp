#include "Log.h"
static ILogSink* g_sink = nullptr;
ILogSink* Log::SetSink(ILogSink* s) { auto* old = g_sink; g_sink = s; return old; }
void Log::Info(const std::string& m) { if (g_sink) g_sink->info(m); }
void Log::Error(const std::string& m) { if (g_sink) g_sink->error(m); }
