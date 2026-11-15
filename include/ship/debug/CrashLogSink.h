#pragma once

#include <mutex>
#include <spdlog/sinks/base_sink.h>

#include "ship/debug/CrashMonitor.h"

namespace Ship {
class CrashLogSink final : public spdlog::sinks::base_sink<std::mutex> {
  protected:
    void sink_it_(const spdlog::details::log_msg& message) override {
        spdlog::memory_buf_t formatted;
        formatter_->format(message, formatted);
        CrashMonitor::AppendLog(formatted.data(), formatted.size());
    }

    void flush_() override {
    }
};
} // namespace Ship
