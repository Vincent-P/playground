#include "timer.hpp"

namespace my_app
{

float TimerData::get_time() const { return float_time; }

float TimerData::get_delta_time() const { return float_delta_time; }

float TimerData::get_average_delta_time() const { return average_delta_time; }

std::array<float, 10> const &TimerData::get_delta_time_histogram() const { return delta_time_histogram; }

float TimerData::get_average_fps() const { return average_fps; }

std::array<float, 10> const &TimerData::get_fps_histogram() const { return fps_histogram; }

void TimerData::update()
{
    {
	auto previous_time = time;
	time               = clock_t::now();
	delta_time         = clock_t::now() - previous_time;
    }
    {
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();
	float_time        = static_cast<float>(milliseconds * 0.001f);
	float_delta_time  = delta_time.count();
    }
    {
	static size_t previous_second = 0;
	size_t current_second         = static_cast<size_t>(float_time) % (2);

	if (current_second != previous_second) {
	    average_fps = current_second_fps;
	    for (size_t i = 1; i < fps_histogram.size(); ++i) {
		average_fps += fps_histogram[i];
		fps_histogram[i - 1]        = fps_histogram[i];
		delta_time_histogram[i - 1] = delta_time_histogram[i];
	    }
	    fps_histogram[9]        = current_second_fps;
	    delta_time_histogram[9] = 1000.0f / current_second_fps;
	    average_fps *= 0.1f;
	    average_delta_time = 1000.0f / average_fps;
	    current_second_fps = 0.0f;
	}
	current_second_fps += 1.0f;
	previous_second = current_second;
    }
}

TimerData::TimerData()
    : time(clock_t::now()), delta_time(clock_t::now() - clock_t::now()), float_delta_time(10.0f),
      average_delta_time(10.0f), average_fps(10.0f), current_second_fps(10.0f)
{

    for (size_t i = 0; i < fps_histogram.size(); ++i) {
        fps_histogram[i]        = 10.0f;
        delta_time_histogram[i] = 10.0f;
    }

    update();
}

TimerData::~TimerData() {}

} // namespace my_app
