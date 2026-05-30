#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <cstddef>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <flint/fmpz_mpoly.h>

class Processor {
 public:
  Processor();
  ~Processor();
  Processor(const Processor&) = delete;
  Processor(Processor&&) = delete;
  Processor& operator=(const Processor&) = delete;
  Processor& operator=(Processor&&) = delete;

  void AddVariable(std::string_view name);
  void RemoveVariable(std::string_view name);
  void SetSubstitution(std::string_view name, std::string_view value);
  void ClearSubstitution(std::string_view name);

  struct FlintFreeDeleter {
    void operator()(char* ptr) const noexcept { flint_free(ptr); }
  };

  using FlintString = std::unique_ptr<char, FlintFreeDeleter>;

  FlintString SimpleEvaluate(const char* expr);
  FlintString Evaluate(const char* expr);

 private:
  void UpdateRawVariables(std::vector<std::string>&& new_variables);

  struct Substitution {
    std::string name;
    std::string value;
    std::regex regex;
  };

  std::vector<std::string> variables_;
  std::vector<const char*> raw_variables_;
  std::vector<Substitution> substitutions_;
  fmpz_mpoly_ctx_t context_;

  // For simplicity, the current implementation uses regular expressions in some
  // places, assuming the performance impact is limited. We track the maximum
  // input sizes to verify that the inputs to the relevant routines remain
  // small.
  std::size_t max_substitution_input_size = 0;
  std::size_t max_normalization_input_size = 0;
};

#endif  // PROCESSOR_H
