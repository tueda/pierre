#ifndef CLI_H
#define CLI_H

#include <istream>
#include <ostream>
#include <span>

#include "env.h"

// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
int RunCli(int argc, char* argv[]);
int RunCli(std::span<char* const> args, std::istream& input,
           std::ostream& output, std::ostream& error, const Environment& env);

#endif  // CLI_H
