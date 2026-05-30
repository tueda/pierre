#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "processor.h"

TEST_CASE("AddVariable error 1") {
  Processor processor;
  CHECK_THROWS_WITH([&]() { processor.AddVariable(""); }(),
                    "variable name cannot be empty");
}

TEST_CASE("AddVariable error 2") {
  Processor processor;
  processor.AddVariable("x");
  CHECK_THROWS_WITH([&]() { processor.AddVariable("x"); }(),
                    "variable already exists");
}

TEST_CASE("RemoveVariable error 1") {
  Processor processor;
  CHECK_THROWS_WITH([&]() { processor.RemoveVariable(""); }(),
                    "variable name cannot be empty");
}

TEST_CASE("RemoveVariable error 2") {
  Processor processor;
  CHECK_THROWS_WITH([&]() { processor.RemoveVariable("x"); }(),
                    "variable does not exist");
}

TEST_CASE("SetSubstitution 1") {
  Processor processor;
  processor.AddVariable("x");
  processor.AddVariable("y");
  processor.SetSubstitution("x", "-3/2");
  auto result = processor.Evaluate("1/y + 1/(1-x) + 1/(x-1)/y");
  CHECK(result.get() != nullptr);
  CHECK(std::string_view{result.get()} == "(2*y+3)/(5*y)");
}

TEST_CASE("SetSubstitution 2") {
  Processor processor;
  processor.AddVariable("x");
  processor.AddVariable("y");
  processor.SetSubstitution("x", "-3/2");
  processor.SetSubstitution("x", "1/2");
  auto result = processor.Evaluate("1/y + 1/(1-x) + 1/(x-1)/y");
  CHECK(result.get() != nullptr);
  CHECK(std::string_view{result.get()} == "(2*y-1)/(y)");
}

TEST_CASE("SetSubstitution error 1") {
  Processor processor;
  CHECK_THROWS_WITH([&]() { processor.SetSubstitution("", "1"); }(),
                    "variable name cannot be empty");
}

TEST_CASE("ClearSubstitution 1") {
  Processor processor;
  processor.AddVariable("x");
  processor.AddVariable("y");
  processor.SetSubstitution("x", "-3/2");
  processor.SetSubstitution("y", "1/2");
  processor.ClearSubstitution("x");
  processor.ClearSubstitution("y");
  auto result = processor.Evaluate("1/y + 1/(1-x) + 1/(x-1)/y");
  CHECK(result.get() != nullptr);
  CHECK(std::string_view{result.get()} == "(x-y)/(x*y-y)");
}

TEST_CASE("ClearSubstitution error 1") {
  Processor processor;
  CHECK_THROWS_WITH([&]() { processor.ClearSubstitution(""); }(),
                    "variable name cannot be empty");
}

TEST_CASE("ClearSubstitution error 2") {
  Processor processor;
  CHECK_THROWS_WITH([&]() { processor.ClearSubstitution("x"); }(),
                    "variable is not associated with any substitution");
}

TEST_CASE("Evaluate 1") {
  Processor processor;
  processor.AddVariable("x");
  auto result = processor.Evaluate("2/(1+x) - 1");
  CHECK(result.get() != nullptr);
  CHECK(std::string_view{result.get()} == "(-x+1)/(x+1)");
}

TEST_CASE("Evaluate 2") {
  Processor processor;
  processor.AddVariable("x");
  processor.AddVariable("y");
  auto result = processor.Evaluate("1/y + 1/(1-x) + 1/(x-1)/y");
  CHECK(result.get() != nullptr);
  CHECK(std::string_view{result.get()} == "(x-y)/(x*y-y)");
}

TEST_CASE("Evaluate 3") {
  Processor processor;
  processor.AddVariable("x");
  processor.AddVariable("y");
  processor.RemoveVariable("x");
  auto result = processor.Evaluate("y + 1/y");
  CHECK(result.get() != nullptr);
  CHECK(std::string_view{result.get()} == "(y^2+1)/(y)");
}

TEST_CASE("Evaluate requiring power normalization 1") {
  Processor processor;
  processor.AddVariable("x");
  auto result = processor.Evaluate("x ^ 3");
  CHECK(result.get() != nullptr);
  CHECK(std::string_view{result.get()} == "x^3");
}

TEST_CASE("Evaluate requiring power normalization 2") {
  Processor processor;
  processor.AddVariable("x");
  auto result = processor.Evaluate("x ^ ( 6 / 2 )");
  CHECK(result.get() != nullptr);
  CHECK(std::string_view{result.get()} == "x^3");
}

TEST_CASE("Evaluate requiring power normalization 3-1") {
  Processor processor;
  processor.AddVariable("x");
  auto result = processor.Evaluate("x ^ - 3");
  CHECK(result.get() != nullptr);
  CHECK(std::string_view{result.get()} == "(1)/(x^3)");
}

TEST_CASE("Evaluate requiring power normalization 3-2") {
  Processor processor;
  auto result = processor.Evaluate("3 ^ - 3");
  CHECK(result.get() != nullptr);
  CHECK(std::string_view{result.get()} == "(1)/27");
}

TEST_CASE("Evaluate requiring power normalization 4-1") {
  Processor processor;
  processor.AddVariable("x");
  auto result = processor.Evaluate("x ^ ( - 6 / 2 )");
  CHECK(result.get() != nullptr);
  CHECK(std::string_view{result.get()} == "(1)/(x^3)");
}

TEST_CASE("Evaluate requiring power normalization 4-2") {
  Processor processor;
  auto result = processor.Evaluate("3 ^ ( - 6 / 2 )");
  CHECK(result.get() != nullptr);
  CHECK(std::string_view{result.get()} == "(1)/27");
}

TEST_CASE("SimpleEvaluate error 1") {
  Processor processor;
  CHECK_THROWS_WITH([&]() { processor.SimpleEvaluate("x"); }(),
                    "failed to parse rational function");
}

TEST_CASE("Evaluate error 1") {
  Processor processor;
  CHECK_THROWS_WITH([&]() { processor.Evaluate("x"); }(),
                    "failed to parse rational function");
}

TEST_CASE("Evaluate requiring power normalization error 1") {
  Processor processor;
  processor.SetSubstitution("m", "1 / 3");
  processor.SetSubstitution("n", "- 1 / 3");
  CHECK_THROWS_WITH([&]() { processor.Evaluate("3 ^ m"); }(),
                    "failed to parse rational function");
  CHECK_THROWS_WITH([&]() { processor.Evaluate("3 ^ n"); }(),
                    "failed to parse rational function");
}

TEST_CASE("Exponentiation should be right-associative" *
          doctest::should_fail()) {
  Processor processor;
  auto result = processor.Evaluate("3^3^3");
  CHECK(result.get() != nullptr);
  CHECK(std::string_view{result.get()} == "7625597484987");
}
