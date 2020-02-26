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
    ~TimerData();

    void update();

    float get_time() const;
    float get_delta_time() const;
    float get_average_delta_time() const;
    std::array<float, 10> const &get_delta_time_histogram() const;
    float get_average_fps() const;
    std::array<float, 10> const &get_fps_histogram() const;

  private:
    time_t time;
    float float_time;
    duration_t delta_time;
    float float_delta_time;
    float average_delta_time;
    std::array<float, 10> delta_time_histogram;
    std::array<float, 10> fps_histogram;
    float average_fps;
    float current_second_fps;
};

} // namespace my_app
