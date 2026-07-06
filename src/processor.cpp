#include "processor.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <flint/flint.h>
#include <flint/fmpz_mpoly.h>
#include <flint/fmpz_mpoly_q.h>
#include <flint/fmpz_types.h>
#include <flint/mpoly_types.h>

#include <ctre.hpp>  // NOLINT(misc-include-cleaner)
#include <ctre/wrapper.hpp>
#include <gsl/gsl>

#include "logger.h"

namespace {

std::optional<std::string> try_normalize_powers(Processor& processor,
                                                std::string_view expr) {
  std::string output;
  std::size_t pos = 0;

  for (auto match : ctre::search_all<
           "(?:"
           // case 1: ^ 3 --> ^3
           R"(\^[ \t]+([0-9]+))"
           "|"
           // case 2: ^ ( 4 / 2 ) --> ^2
           R"(\^[ \t]*\([ \t]*([0-9]+)[ \t]*/[ \t]*([0-9]+)[ \t]*\))"
           "|"
           // case 3: x ^ - 3 --> (1/x)^3
           R"(\b((?:[A-Za-z][0-9A-Z_a-z]*|[0-9]+))[ \t]*)"
           R"(\^[ \t]*-[ \t]*([0-9]+))"
           "|"
           // case 4: x ^ ( - 4 / 2 ) --> (1/x)^2
           R"(\b((?:[A-Za-z][0-9A-Z_a-z]*|[0-9]+))[ \t]*)"
           R"(\^[ \t]*\([ \t]*-[ \t]*([0-9]+)[ \t]*/[ \t]*([0-9]+)[ \t]*\))"
           ")">(expr)) {
    const auto whole = match.get<0>().to_view();
    const auto begin = static_cast<std::size_t>(whole.data() - expr.data());
    const auto end = begin + whole.size();
    std::string replacement;

    if (match.get<1>()) {
      const auto exponent = match.get<1>().to_view();
      replacement = std::string{"^"}.append(exponent);
    } else if (match.get<2>()) {
      const auto numerator = match.get<2>().to_view();
      const auto denominator = match.get<3>().to_view();

      const std::string exponent = processor
                                       .SimpleEvaluate(std::string{numerator}
                                                           .append("/")
                                                           .append(denominator)
                                                           .c_str())
                                       .get();
      if (exponent.find('/') != std::string_view::npos) {
        continue;
      }
      replacement = std::string{"^"}.append(exponent);
    } else if (match.get<4>()) {
      const auto symbol = match.get<4>().to_view();
      const auto exponent = match.get<5>().to_view();
      replacement =
          std::string{"(1/"}.append(symbol).append(")^").append(exponent);
    } else {
      const auto symbol = match.get<6>().to_view();
      const auto numerator = match.get<7>().to_view();
      const auto denominator = match.get<8>().to_view();
      const std::string exponent = processor
                                       .SimpleEvaluate(std::string{numerator}
                                                           .append("/")
                                                           .append(denominator)
                                                           .c_str())
                                       .get();
      if (exponent.find('/') != std::string_view::npos) {
        continue;
      }
      replacement =
          std::string{"(1/"}.append(symbol).append(")^").append(exponent);
    }
    if (replacement == whole) {
      continue;
    }
    output.append(expr.substr(pos, begin - pos));
    output.append(replacement);
    pos = end;
  }

  if (pos == 0) {
    return std::nullopt;
  }

  output.append(expr.substr(pos));

  LOG_DEBUG("before normalization : {}", expr);
  LOG_DEBUG("after normalization  : {}", output);

  return output;
}

}  // namespace

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
Processor::Processor() { fmpz_mpoly_ctx_init(context_, 0, ORD_DEGREVLEX); }

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
Processor::~Processor() { fmpz_mpoly_ctx_clear(context_); }

void Processor::AddVariable(std::string_view name) {
  if (name.empty()) {
    throw std::runtime_error("variable name cannot be empty");
  }

  if (std::ranges::find(variables_, name) != variables_.end()) {
    throw std::runtime_error("variable already exists");
  }

  std::vector<std::string> new_variables{variables_};
  new_variables.emplace_back(name);

  UpdateRawVariables(std::move(new_variables));
}

void Processor::RemoveVariable(std::string_view name) {
  if (name.empty()) {
    throw std::runtime_error("variable name cannot be empty");
  }

  if (std::ranges::find(variables_, name) == variables_.end()) {
    throw std::runtime_error("variable does not exist");
  }

  std::vector<std::string> new_variables{variables_};
  std::erase(new_variables, std::string{name});

  UpdateRawVariables(std::move(new_variables));
}

void Processor::UpdateRawVariables(std::vector<std::string>&& new_variables) {
  std::vector<const char*> new_raw_variables;
  new_raw_variables.reserve(new_variables.size());

  for (const auto& variable : new_variables) {
    new_raw_variables.push_back(variable.c_str());
  }

  auto nvars = gsl::narrow<slong>(new_variables.size());

  fmpz_mpoly_ctx_clear(context_);
  fmpz_mpoly_ctx_init(context_, nvars, ORD_DEGREVLEX);

  variables_ = std::move(new_variables);
  raw_variables_ = std::move(new_raw_variables);
}

void Processor::SetSubstitution(std::string_view name, std::string_view value) {
  if (name.empty()) {
    throw std::runtime_error("variable name cannot be empty");
  }

  Substitution item{.name = std::string{name},
                    .value = "(" + std::string{value} + ")",
                    .regex = std::regex("\\b" + std::string{name} + "\\b")};

  auto iter = std::ranges::find(substitutions_, name,
                                [](const auto& substitution) -> const auto& {
                                  return substitution.name;
                                });

  if (iter != substitutions_.end()) {
    *iter = std::move(item);
  } else {
    substitutions_.push_back(std::move(item));
  }
}

void Processor::ClearSubstitution(std::string_view name) {
  if (name.empty()) {
    throw std::runtime_error("variable name cannot be empty");
  }

  auto iter = std::ranges::find(substitutions_, name,
                                [](const auto& substitution) -> const auto& {
                                  return substitution.name;
                                });

  if (iter == substitutions_.end()) {
    throw std::runtime_error(
        "variable is not associated with any substitution");
  }

  substitutions_.erase(iter);
}

Processor::FlintString Processor::SimpleEvaluate(const char* expr) {
  fmpz_mpoly_q_t rat;
  fmpz_mpoly_q_init(rat, context_);

  auto cleanup_q = gsl::finally([&] { fmpz_mpoly_q_clear(rat, context_); });

  const int res =
      fmpz_mpoly_q_set_str_pretty(rat, expr, raw_variables_.data(), context_);

  if (res != 0) {
    throw std::runtime_error("failed to parse rational function");
  }

  char* out = fmpz_mpoly_q_get_str_pretty(rat, raw_variables_.data(), context_);

  if (out == nullptr) {
    throw std::runtime_error("failed to construct string representation");
  }

  return FlintString{out};
}

Processor::FlintString Processor::Evaluate(const char* expr) {
  fmpz_mpoly_q_t rat;
  fmpz_mpoly_q_init(rat, context_);

  auto cleanup_q = gsl::finally([&] { fmpz_mpoly_q_clear(rat, context_); });

  int res;  // NOLINT(cppcoreguidelines-init-variables)

  auto second_try = [this, &rat](std::string_view input) {
    auto normalized = try_normalize_powers(*this, input);
    if (!normalized) {
      return -1;
    }
    std::size_t input_size = input.size();
    if (input_size > max_normalization_input_size) {
      max_normalization_input_size = input_size;
      LOG_DEBUG("perf: max normalization input size updated: {}", input_size);
    }
    return fmpz_mpoly_q_set_str_pretty(rat, normalized->c_str(),
                                       raw_variables_.data(), context_);
  };

  if (!substitutions_.empty()) {
    std::string substituted = expr;
    std::size_t input_size = substituted.size();

    if (input_size > max_substitution_input_size) {
      max_substitution_input_size = input_size;
      LOG_DEBUG("perf: max substitution input size updated: {}", input_size);
    }

    for (const auto& substitution : substitutions_) {
      substituted = std::regex_replace(substituted, substitution.regex,
                                       substitution.value);
    }

    if (substituted != expr) {
      LOG_DEBUG("before substitution : {}", expr);
      LOG_DEBUG("after substitution  : {}", substituted);
    }

    res = fmpz_mpoly_q_set_str_pretty(rat, substituted.c_str(),
                                      raw_variables_.data(), context_);
    if (res != 0) {
      res = second_try(substituted);
    }
  } else {
    res =
        fmpz_mpoly_q_set_str_pretty(rat, expr, raw_variables_.data(), context_);
    if (res != 0) {
      res = second_try(expr);
    }
  }

  if (res != 0) {
    throw std::runtime_error("failed to parse rational function");
  }

  char* out = fmpz_mpoly_q_get_str_pretty(rat, raw_variables_.data(), context_);

  if (out == nullptr) {
    throw std::runtime_error("failed to construct string representation");
  }

  return FlintString{out};
}
