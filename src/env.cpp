#include "env.h"

#include <cassert>
#include <cstdlib>

bool Environment::GetBool(const char* name) const {
  const char* value = Get(name);
  // Follow the convention used by NO_COLOR and FORCE_COLOR.
  return value != nullptr && *value != '\0';
}

const char* SystemEnvironment::Get(const char* name) const {
  assert(name != nullptr);
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  return std::getenv(name);
}
