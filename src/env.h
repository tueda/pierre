#ifndef ENV_H
#define ENV_H

#include <optional>
#include <string>

class Environment {
 public:
  Environment() = default;
  virtual ~Environment() = default;
  Environment(const Environment&) = delete;
  Environment(Environment&&) = delete;
  Environment& operator=(const Environment&) = delete;
  Environment& operator=(Environment&&) = delete;

  [[nodiscard]] virtual const char* Get(const char* name) const = 0;

  [[nodiscard]] bool GetBool(const char* name) const;
};

class SystemEnvironment final : public Environment {
 public:
  [[nodiscard]] const char* Get(const char* name) const override;
};

#endif  // ENV_H
