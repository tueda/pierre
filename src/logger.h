#ifndef LOGGER_H
#define LOGGER_H

#define QUILL_DISABLE_NON_PREFIXED_MACROS

#include <quill/Backend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>

void InitLogger(quill::LogLevel stderr_level, quill::LogLevel file_level);
quill::Logger* GetLogger(quill::LogLevel level);
quill::Logger* GetStderrOnlyLogger(quill::LogLevel level);
quill::Logger* GetFileOnlyLogger(quill::LogLevel level);

#define LOG_DEBUG(fmt, ...)                                             \
  do {                                                                  \
    if (auto* logger = GetLogger(quill::LogLevel::Debug)) {             \
      QUILL_LOG_DEBUG(logger, fmt, ##__VA_ARGS__);                      \
      if (auto* logger = GetStderrOnlyLogger(quill::LogLevel::Debug)) { \
        quill::Backend::notify();                                       \
        logger->flush_log();                                            \
      }                                                                 \
    }                                                                   \
  } while (0)

#define LOG_INFO(fmt, ...)                                             \
  do {                                                                 \
    if (auto* logger = GetLogger(quill::LogLevel::Info)) {             \
      QUILL_LOG_INFO(logger, fmt, ##__VA_ARGS__);                      \
      if (auto* logger = GetStderrOnlyLogger(quill::LogLevel::Info)) { \
        quill::Backend::notify();                                      \
        logger->flush_log();                                           \
      }                                                                \
    }                                                                  \
  } while (0)

#define LOG_ERROR(fmt, ...)                                             \
  do {                                                                  \
    if (auto* logger = GetLogger(quill::LogLevel::Error)) {             \
      QUILL_LOG_ERROR(logger, fmt, ##__VA_ARGS__);                      \
      if (auto* logger = GetStderrOnlyLogger(quill::LogLevel::Error)) { \
        quill::Backend::notify();                                       \
        logger->flush_log();                                            \
      }                                                                 \
    }                                                                   \
  } while (0)

#define LOG_FATAL(fmt, ...)                                                \
  do {                                                                     \
    if (auto* logger = GetLogger(quill::LogLevel::Critical)) {             \
      QUILL_LOG_CRITICAL(logger, fmt, ##__VA_ARGS__);                      \
      if (auto* logger = GetStderrOnlyLogger(quill::LogLevel::Critical)) { \
        quill::Backend::notify();                                          \
        logger->flush_log();                                               \
      }                                                                    \
    }                                                                      \
  } while (0)

#define LOGFILE_INFO(fmt, ...)                                     \
  do {                                                             \
    if (auto* logger = GetFileOnlyLogger(quill::LogLevel::Info)) { \
      QUILL_LOG_INFO(logger, fmt, ##__VA_ARGS__);                  \
    }                                                              \
  } while (0)

#endif  // LOGGER_H
