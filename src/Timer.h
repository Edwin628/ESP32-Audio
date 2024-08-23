// Timer.h
#ifndef TIMER_H
#define TIMER_H

#include <chrono>

class Timer {
public:
    Timer();  // 构造函数
    void start();
    void stop();
    void reset();
    long long getTotalDuration() const;

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    long long total_duration;
    bool timing;
};

#endif // TIMER_H