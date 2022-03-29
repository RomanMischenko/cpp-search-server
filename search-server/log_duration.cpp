#include "log_duration.h"

LogDuration::LogDuration(const std::string& id, const std::ostream& out = std::cerr)
        : id_(id), out_(out)
    {}

LogDuration::~LogDuration() {
        using namespace std::chrono;
        using namespace std::literals;

        const auto end_time = Clock::now();
        const auto dur = end_time - start_time_;
        std::cerr << id_ << ": "s << duration_cast<milliseconds>(dur).count() << " ms"s << std::endl;
    }