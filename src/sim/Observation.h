#pragma once
#include <functional>
#include <cstdint>
namespace MQSimObservation {
// Passive, process-local research hook. Never schedules simulator work.
using Sink=std::function<void(const char*,uint64_t,uint64_t,unsigned,unsigned,unsigned)>;
inline Sink sink;
inline void interval(const char* kind,uint64_t start,uint64_t end,unsigned channel,unsigned chip,unsigned die) {
    if(sink) sink(kind,start,end,channel,chip,die);
}
}
