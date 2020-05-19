#pragma once

#include <array>
#include <chrono>

namespace my_app
{

using clock_t    = std::chrono::high_resolution_clock;
using time_t     = std::chrono::time_point<clock_t>;
using duration_t = std::chrono::duration<float>;

class TimerData
{
  public:
    TimerData();
    ~TimerData() = default;

    void update();

    [[nodiscard]] float get_time() const;
    [[nodiscard]] float get_delta_time() const;
    [[nodiscard]] float get_average_delta_time() const;
    [[nodiscard]] std::array<float, 10> const &get_delta_time_histogram() const;
    [[nodiscard]] float get_average_fps() const;
    [[nodiscard]] std::array<float, 10> const &get_fps_histogram() const;

  private:
    time_t time;
    float float_time;
    duration_t delta_time;
    float float_delta_time{10.0f};
    float average_delta_time{10.0f};
    std::array<float, 10> delta_time_histogram;
    std::array<float, 10> fps_histogram;
    float average_fps{10.0f};
    float current_second_fps{10.0f};
};

} // namespace my_app
