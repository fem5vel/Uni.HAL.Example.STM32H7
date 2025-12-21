// src_lib/perf/ox_perf_stats.h

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct {
        uint64_t total_ticks;
        uint64_t total_bytes;
        uint32_t min_ticks;
        uint32_t max_ticks;
        uint32_t call_count;
        uint32_t last_start_tick;
    } ox_perf_counter_t;

    void ox_perf_counter_reset(ox_perf_counter_t* counter);

    void ox_perf_counter_start(ox_perf_counter_t* counter);

    void ox_perf_counter_stop(ox_perf_counter_t* counter, size_t bytes_processed);

    void ox_perf_counter_print(const ox_perf_counter_t* counter, const char* name, uint32_t cpu_freq_hz);

#ifdef __cplusplus
}
#endif