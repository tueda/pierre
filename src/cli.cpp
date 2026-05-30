#include "cli.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>

#include <flint/flint.h>

#include <ctre.hpp>  // NOLINT(misc-include-cleaner)
#include <ctre/wrapper.hpp>
#include <quill/bundled/fmt/format.h>
#include <quill/core/LogLevel.h>

#include "env.h"
#include "logger.h"
#include "processor.h"
#include "timer.h"
#include "version.h"

namespace {

inline constexpr std::string_view whitespace_chars = " \t\n\v\f\r";

constexpr bool is_identifier_start(char character) {
  return ('A' <= character && character <= 'Z') ||
         ('a' <= character && character <= 'z');
}

constexpr bool is_identifier_continue(char character) {
  return ('0' <= character && character <= '9') ||
         ('A' <= character && character <= 'Z') || character == '_' ||
         ('a' <= character && character <= 'z');
}

std::string_view string_view_from_pos(const std::string& str,
                                      std::size_t begin_pos,
                                      std::size_t end_pos) {
  assert(begin_pos >= 0);
  assert(begin_pos <= str.size());
  assert(end_pos >= 0);
  assert(end_pos <= str.size());
  assert(begin_pos <= end_pos);
  return {str.data() + begin_pos, end_pos - begin_pos};
}

struct CliSession {
  std::span<char* const> args;
  std::istream& input;
  std::ostream& output;
  std::ostream& error;
  const Environment& env;

  Processor processor;
  Timer timer;
  std::string prompt = ">";
  std::string line;  // reused to avoid repeated allocations
  int display_constant = 3;
  bool timing = true;
  bool ugly_display = false;
  bool suppress_long_poly = true;

  // Making this function constexpr causes a compile-time error with GCC 11.
  [[nodiscard]] bool ShouldTerminateSessionOnError() const {
    // The peer is most likely automated. Terminate the session on error instead
    // of attempting recovery to avoid a deadlock.
    return prompt.empty() && !timing && ugly_display && !suppress_long_poly;
  }
};

void init_logger(CliSession& cli) {
  const bool logfile_enabled = cli.env.GetBool("PIERRE_LOG");
  const bool debug_enabled = cli.env.GetBool("PIERRE_DEBUG");

  quill::LogLevel stderr_level = quill::LogLevel::Error;
  quill::LogLevel file_level = quill::LogLevel::None;
  if (logfile_enabled) {
    if (debug_enabled) {
      stderr_level = quill::LogLevel::Debug;
    }
    file_level = quill::LogLevel::Debug;
  } else {
    if (debug_enabled) {
      stderr_level = quill::LogLevel::Debug;
    }
  }

  InitLogger(stderr_level, file_level);
}

void print_version(std::ostream& output) {
  const auto app_version_line =
      fmtquill::format("Pierre version {}", project_version);

  output << app_version_line << '\n';
  LOGFILE_INFO("{}", app_version_line);

  const auto flint_build_version =
      fmtquill::format("{}.{}.{}", __FLINT_VERSION, __FLINT_VERSION_MINOR,
                       __FLINT_VERSION_PATCHLEVEL);
  auto* const flint_runtime_version = flint_version;

  auto flint_version_line =
      fmtquill::format("Built on FLINT version {}", flint_build_version);

  if (flint_build_version != flint_runtime_version) {
    flint_version_line +=
        fmtquill::format(" (runtime: {})", flint_runtime_version);
  }

  output << flint_version_line << '\n';
  LOGFILE_INFO("{}", flint_version_line);
}

void print_date(std::ostream& output) {
  std::time_t now = std::time(nullptr);
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  output << ' ' << std::ctime(&now);
}

std::optional<std::reference_wrapper<std::string>> get_next_line(
    CliSession& cli) {
  if (!cli.prompt.empty()) {
    cli.output << cli.prompt;
  }
  cli.output << std::flush;

  cli.timer.record_elapsed_time();

  if (!std::getline(cli.input, cli.line)) {
    cli.timer.restart();
    if (!cli.prompt.empty()) {
      cli.output << '\n' << std::flush;
    }
    LOG_DEBUG("IN  : <EOF>");
    return std::nullopt;
  }

  cli.timer.restart();
  LOG_DEBUG("IN  : {}", cli.line);
  return std::ref(cli.line);
}

std::optional<std::tuple<std::size_t, std::size_t, std::size_t, std::size_t>>
find_substitution(std::string_view line) {
  // var:=something
  const std::size_t var_begin = line.find_first_not_of(whitespace_chars);
  if (var_begin == std::string_view::npos) {
    return std::nullopt;
  }
  if (!is_identifier_start(line[var_begin])) {
    return std::nullopt;
  }
  std::size_t pos = var_begin + 1;
  for (;;) {
    if (pos == line.size()) {
      return std::nullopt;
    }
    if (!is_identifier_continue(line[pos])) {
      break;
    }
    pos++;
  }
  const std::size_t var_end = pos;
  const std::size_t op_begin =
      line.find_first_not_of(whitespace_chars, var_end);
  if (op_begin == std::string_view::npos) {
    return std::nullopt;
  }
  if (line.compare(op_begin, 2, ":=") != 0) {
    return std::nullopt;
  }
  const std::size_t value_begin =
      line.find_first_not_of(whitespace_chars, op_begin + 2);
  if (value_begin == std::string_view::npos) {
    return std::nullopt;
  }
  const std::size_t value_end = line.find_last_not_of(whitespace_chars) + 1;
  assert(value_begin < value_end);
  return std::make_tuple(var_begin, var_end, value_begin, value_end);
}

bool process_ampersand_command(std::string_view command, CliSession& cli) {
  // &d
  if (ctre::match<R"(&\s*d)">(command)) {
    cli.output << "Enter display size.\n";

    auto next_line = get_next_line(cli);
    if (!next_line) {
      return false;
    }

    auto value = next_line.value().get();
    int number = -1;

    try {
      number = std::stoi(value);
      if (number >= 0) {
        value = std::to_string(number);
      }
    } catch (std::invalid_argument const& e) {
      number = -1;
    } catch (std::out_of_range const& e) {
      number = -1;
    }

    cli.display_constant = number;
    cli.output << " Ignored: " << value << "\n\n        0\n\n";
    return true;
  }
  // &(J=var)
  if (auto match =
          ctre::match<R"(&\s*\(\s*J\s*=\s*([A-Za-z][0-9A-Z_a-z]*)\s*\))">(
              command)) {
    const std::string_view name = match.get<1>().to_view();
    cli.processor.AddVariable(name);
    cli.output << " Add Variable: " << name << "\n\n        0\n\n";
    return true;
  }
  // &(J=-var)
  if (auto match =
          ctre::match<R"(&\s*\(\s*J\s*=\s*-\s*([A-Za-z][0-9A-Z_a-z]*)\s*\))">(
              command)) {
    const std::string_view name = match.get<1>().to_view();
    cli.processor.RemoveVariable(name);
    cli.output << " Remove Variable: " << name << "\n\n        0\n\n";
    return true;
  }
  // &(M=' ')
  if (ctre::match<R"(&\s*\(\s*M\s*=\s*' '\s*\))">(command)) {
    cli.prompt = "";
    cli.output << " Prompt now: <cr>\n\n        0\n\n";
    return true;
  }
  // &q
  if (ctre::match<R"(&\s*q)">(command)) {
    cli.output << "\n bye\n";
    return false;
  }
  // &(t=0)
  if (ctre::match<R"(&\s*\(\s*t\s*=\s*0\s*\))">(command)) {
    cli.timing = false;
    cli.output << " Timing disabled.\n\n        0\n\n";
    return true;
  }
  // &U
  if (ctre::match<R"(&\s*U)">(command)) {
    cli.ugly_display = true;
    cli.output << " Ignored.\n\n        0\n\n";
    return true;
  }
  // &(_o=n)
  if (ctre::match<R"(&\s*\(\s*_o\s*=\s*\d+\s*\))">(command)) {
    cli.output << " Ignored.\n\n        0\n\n";
    return true;
  }
  // &(_s=i), &(_t=i)
  if (auto match =
          ctre::match<R"(&\s*\(\s*(_[st])\s*=\s*([01])\s*\))">(command)) {
    const std::string_view name = match.get<1>().to_view();
    const bool value = match.get<2>().to_view() != "0";
    if (name == "_s") {
      cli.suppress_long_poly = value;
    }
    cli.output << " Ignored.\n\n        0\n\n";
    return true;
  }
  throw std::runtime_error("command not recognized");
}

bool process_command(std::string& line, CliSession& cli) {
  const std::size_t command_begin = line.find_first_not_of(whitespace_chars);
  if (command_begin == std::string_view::npos) {
    cli.output << '\n';
    return true;
  }
  const std::size_t command_end = line.find_last_not_of(whitespace_chars) + 1;
  assert(command_begin < command_end);

  auto command = string_view_from_pos(line, command_begin, command_end);

  switch (command.front()) {
    case '&':
      return process_ampersand_command(command, cli);
    case '@':
      // @var
      if (auto match = ctre::match<R"(@\s*([A-Za-z][0-9A-Z_a-z]*))">(command)) {
        const std::string_view name = match.get<1>().to_view();
        cli.processor.ClearSubstitution(name);
        cli.output << "\n        0\n\n";
        return true;
      }
      // @(var)
      if (auto match = ctre::match<R"(@\s*\(\s*([A-Za-z][0-9A-Z_a-z]*)\s*\))">(
              command)) {
        const std::string_view name = match.get<1>().to_view();
        cli.processor.ClearSubstitution(name);
        cli.output << "\n        0\n\n";
        return true;
      }
      break;
    default:
      // var:=rational_number
      if (auto subst = find_substitution(line)) {
        auto var_begin = std::get<0>(subst.value());
        auto var_end = std::get<1>(subst.value());
        auto value_begin = std::get<2>(subst.value());
        auto value_end = std::get<3>(subst.value());
        auto var = string_view_from_pos(line, var_begin, var_end);
        auto value = string_view_from_pos(line, value_begin, value_end);

        if (std::ranges::any_of(value, [](char character) {
              return is_identifier_start(character);
            })) {
          throw std::runtime_error(
              "current implementation doesn't support RHS containing "
              "variables");
        }

        if (value_end < line.size()) {
          line[value_end] = '\0';
        }
        auto result = cli.processor.Evaluate(line.data() + value_begin);
        cli.processor.SetSubstitution(var, result.get());
        cli.output << "\n " << result.get() << "\n\n";
        return true;
      }
      // simplification
      if (command_end < line.size()) {
        line[command_end] = '\0';
      }
      auto result = cli.processor.Evaluate(line.data() + command_begin);
      LOG_DEBUG("OUT : {}", result.get());
      cli.output << "\n " << result.get() << "\n\n";
      return true;
  }
  throw std::runtime_error("command not recognized");
}

void log_total_elapsed_time(CliSession& cli) {
  cli.timer.record_elapsed_time();
  LOG_INFO("Total elapsed CPU time: {}",
           seconds_string(cli.timer.total_elapsed_time()));
}

}  // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
int RunCli(int argc, char* argv[]) {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  return RunCli(std::span<char* const>{argv, static_cast<std::size_t>(argc)},
                std::cin, std::cout, std::cerr, SystemEnvironment{});
}

int RunCli(std::span<char* const> args, std::istream& input,
           std::ostream& output, std::ostream& error, const Environment& env) {
  CliSession cli{.args = args,
                 .input = input,
                 .output = output,
                 .error = error,
                 .env = env};

  init_logger(cli);
  print_version(cli.output);
  cli.output << '\n';
  print_date(cli.output);
  cli.output << '\n';

  for (;;) {
    cli.timer.record_elapsed_time();
    if (cli.timing) {
      cli.output << " Elapsed CPU time: "
                 << seconds_string(cli.timer.current_elapsed_time()) << "\n\n";
    }

    auto line = get_next_line(cli);
    cli.timer.record_elapsed_time();
    cli.timer.reset_elapsed_time();

    if (!line) {
      break;
    }

    try {
      if (!process_command(line.value(), cli)) {
        break;
      }
    } catch (const std::runtime_error& e) {
      cli.output << '\n' << std::flush;
      if (cli.ShouldTerminateSessionOnError()) {
        LOG_FATAL("{}", e.what());
        log_total_elapsed_time(cli);
        return EXIT_FAILURE;
      }

      LOG_ERROR("{}", e.what());
      cli.output << " \n\n";
    }
  }

  log_total_elapsed_time(cli);
  return EXIT_SUCCESS;
}
