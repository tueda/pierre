#include "logger.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/Logger.h>
#include <quill/backend/BackendOptions.h>
#include <quill/core/LogLevel.h>
#include <quill/core/PatternFormatterOptions.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>
#include <quill/sinks/Sink.h>

namespace {

#ifndef NDEBUG
bool logger_initialized = false;
#endif

quill::Logger* cached_logger = nullptr;
quill::Logger* cached_stderr_only_logger = nullptr;
quill::Logger* cached_file_only_logger = nullptr;

std::string MakeLogFilename() {
  auto now = std::chrono::system_clock::now();
  const time_t time_value = std::chrono::system_clock::to_time_t(now);
  std::tm calendar_time;

#ifdef _WIN32
  if (localtime_s(&calendar_time, &time_value) != 0) {
    throw std::runtime_error("localtime_s failed");
  }
#else
  if (localtime_r(&time_value, &calendar_time) == nullptr) {
    throw std::runtime_error("localtime_r failed");
  }
#endif

  auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
                         now.time_since_epoch()) %
                     std::chrono::seconds{1};

  constexpr int width_us = 6;

  std::ostringstream oss;
  oss << "pierre_" << std::put_time(&calendar_time, "%Y-%m-%d_%H-%M-%S") << "_"
      << std::setw(width_us) << std::setfill('0') << duration_us.count()
      << "_pid" <<
#ifdef _WIN32
      _getpid()
#else
      getpid()
#endif
      << ".log";
  return oss.str();
}

quill::Logger* GetFilteredLogger(quill::Logger* logger, quill::LogLevel level) {
  if (logger != nullptr && logger->get_log_level() > level) {
    return nullptr;
  }
  return logger;
}

}  // namespace

void InitLogger(quill::LogLevel stderr_level, quill::LogLevel file_level) {
#ifndef NDEBUG
  assert(!logger_initialized);
  logger_initialized = true;
#endif

  if (stderr_level == quill::LogLevel::None &&
      file_level == quill::LogLevel::None) {
    return;
  }

  constexpr auto backend_sleep_duration = std::chrono::milliseconds{100};
  quill::BackendOptions backend_options;
  backend_options.sleep_duration = backend_sleep_duration;
  backend_options.log_level_descriptions[static_cast<std::size_t>(
      quill::LogLevel::Critical)] = "FATAL";
  backend_options.log_level_short_codes[static_cast<std::size_t>(
      quill::LogLevel::Critical)] = "F";
  quill::Backend::start(backend_options);

  std::shared_ptr<quill::Sink> stderr_sink;
  std::shared_ptr<quill::Sink> file_sink;

  if (stderr_level != quill::LogLevel::None) {
    quill::ConsoleSinkConfig config;
    config.set_stream("stderr");
    config.set_colour_mode(quill::ConsoleSinkConfig::ColourMode::Automatic);
    config.set_override_pattern_formatter_options(
        quill::PatternFormatterOptions{
            "%(time)  %(log_level:<5)  %(short_source_location:<18) %(message)",
            "%H:%M:%S.%Qms"});
    stderr_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>(
        "errsink", config);
    stderr_sink->set_log_level_filter(stderr_level);
  }

  if (file_level != quill::LogLevel::None) {
    quill::FileSinkConfig config;
    config.set_override_pattern_formatter_options(
        quill::PatternFormatterOptions{
            "%(time)  %(log_level:<5)  %(short_source_location:<18) %(message)",
            "%F %H:%M:%S.%Qus"});
    file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(
        MakeLogFilename(), config);
    file_sink->set_log_level_filter(file_level);
  }

  if (stderr_sink || file_sink) {
    std::vector<std::shared_ptr<quill::Sink>> sinks;
    if (stderr_sink) {
      sinks.push_back(stderr_sink);
    }
    if (file_sink) {
      sinks.push_back(file_sink);
    }
    cached_logger =
        quill::Frontend::create_or_get_logger("root", std::move(sinks));
    cached_logger->set_log_level(std::min(stderr_level, file_level));
  }

  if (stderr_sink) {
    cached_stderr_only_logger =
        quill::Frontend::create_or_get_logger("stderr_only", stderr_sink);
    cached_stderr_only_logger->set_log_level(stderr_level);
  }

  if (file_sink) {
    cached_file_only_logger =
        quill::Frontend::create_or_get_logger("file_only", file_sink);
    cached_file_only_logger->set_log_level(file_level);
  }
}

quill::Logger* GetLogger(quill::LogLevel level) {
  return GetFilteredLogger(cached_logger, level);
}

quill::Logger* GetStderrOnlyLogger(quill::LogLevel level) {
  return GetFilteredLogger(cached_stderr_only_logger, level);
}

quill::Logger* GetFileOnlyLogger(quill::LogLevel level) {
  return GetFilteredLogger(cached_file_only_logger, level);
}
