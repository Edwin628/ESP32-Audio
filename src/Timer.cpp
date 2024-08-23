// Timer.cpp
#include "Timer.h"

Timer::Timer() : total_duration(0), timing(false) {}

void Timer::start() {
    start_time = std::chrono::high_resolution_clock::now();
    timing = true;
}

void Timer::stop() {
    if (timing) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        total_duration += duration;
        timing = false;
    }
}

void Timer::reset() {
    total_duration = 0;
    timing = false;
}

long long Timer::getTotalDuration() const {
    return total_duration;
}
