#pragma once

#include <ctime>
#include <iostream>

class internal_timer {
public:
    double total_time, start_time;
    bool started;
    int call;
    internal_timer() {
        reset();
    }
    void reset() {
        total_time = 0.0;
        start_time = 0.0;
        call = 0;
        started = false;
    }
    void start() {
        start_time = get_timestamp();
        started = true;
    }
    void end() {
        if (started) {
            total_time += get_timestamp() - start_time;
            call ++;
            started = false;
        }
    }

    auto result() {
        return std::make_pair(total_time, call);
    }

    void print() {
        std::cout << total_time << ' ' << call << std::endl;
    }

// private:
    static double get_timestamp() {
        timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        return ts.tv_sec + ts.tv_nsec / 1e9;
    }
};