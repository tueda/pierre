#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

inline std::string seconds_string(std::chrono::milliseconds elapsed) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(3)
      << std::chrono::duration<double>(elapsed).count();
  return oss.str();
}

class Timer {
 public:
  Timer() { restart(); }

  void restart() { timer_start_time_ = std::chrono::steady_clock::now(); }

  void record_elapsed_time() {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - timer_start_time_);
    elapsed_time_ += duration;
    total_elapsed_time_ += duration;
    timer_start_time_ = end_time;
  }

  void reset_elapsed_time() { elapsed_time_ = std::chrono::milliseconds{0}; }

  [[nodiscard]] std::chrono::milliseconds current_elapsed_time() const {
    return elapsed_time_;
  }

  [[nodiscard]] std::chrono::milliseconds total_elapsed_time() const {
    return total_elapsed_time_;
  }

 private:
  std::chrono::steady_clock::time_point timer_start_time_;
  std::chrono::milliseconds elapsed_time_{0};
  std::chrono::milliseconds total_elapsed_time_{0};
};

#endif  // TIMER_H
