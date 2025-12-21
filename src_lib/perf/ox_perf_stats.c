// src_lib/perf/ox_perf_stats.c

#include "ox_perf_stats.h"
#include "dwt/uni_hal_dwt.h"
#include "io/uni_hal_io_stdio.h"
#include <limits.h>
#include <stdio.h>

void ox_perf_counter_reset(ox_perf_counter_t* counter) {
    if (counter == NULL) return;

    counter->total_ticks = 0;
    counter->total_bytes = 0;
    counter->min_ticks = UINT32_MAX;
    counter->max_ticks = 0;
    counter->call_count = 0;
    counter->last_start_tick = 0;
}

void ox_perf_counter_start(ox_perf_counter_t* counter) {
    if (counter == NULL) return;

    counter->last_start_tick = uni_hal_dwt_get_tick();
}

void ox_perf_counter_stop(ox_perf_counter_t* counter, size_t bytes_processed) {
    if (counter == NULL) return;

    uint32_t end_tick = uni_hal_dwt_get_tick();
    uint32_t elapsed_ticks = end_tick - counter->last_start_tick;

    counter->total_ticks += elapsed_ticks;
    counter->total_bytes += bytes_processed;
    counter->call_count++;

    if (elapsed_ticks < counter->min_ticks) {
        counter->min_ticks = elapsed_ticks;
    }
    if (elapsed_ticks > counter->max_ticks) {
        counter->max_ticks = elapsed_ticks;
    }
}

void ox_perf_counter_print(const ox_perf_counter_t* counter, const char* name, uint32_t cpu_freq_hz) {
    if (counter == NULL || counter->call_count == 0) {
        uni_hal_io_stdio_printf("[PERF] %s: No data\r\n", name);
        return;
    }

    // Время в микросекундах
    uint32_t ticks_per_us = cpu_freq_hz / 1000000U;
    uint32_t total_time_us = 0;
    if (ticks_per_us > 0) {
        total_time_us = (uint32_t)(counter->total_ticks / ticks_per_us);
    }

    // Скорость в байт/сек
    uint32_t throughput_bps = 0;
    if (total_time_us > 0) {
        throughput_bps = (uint32_t)((counter->total_bytes * 1000000ULL) / total_time_us);
    }

    // Скорость в КБ/сек
    uint32_t throughput_kbps = throughput_bps / 1024;

    // Такты на байт
    uint32_t ticks_per_byte = 0;
    if (counter->total_bytes > 0) {
        ticks_per_byte = (uint32_t)(counter->total_ticks / counter->total_bytes);
    }

    uni_hal_io_stdio_printf("[PERF] === %s ===\r\n", name);
    uni_hal_io_stdio_printf("[PERF] T/byte: %lu\r\n", (unsigned long)ticks_per_byte);
    uni_hal_io_stdio_printf("[PERF] Speed: %lu KB/s\r\n", (unsigned long)throughput_kbps);
}
