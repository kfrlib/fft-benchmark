#pragma once
/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016 D Levin
 * Benchmark source code is dual-licensed under MIT and GPL 2 or later
 * See LICENSE.txt for details
 */

#include <algorithm>
#include <stdlib.h>
#include <vector>

typedef unsigned long long tick_value;
typedef unsigned long long time_value;

template <typename T>
static T* aligned_malloc(size_t size, size_t alignment = 64)
{
    void* ptr = malloc(size * sizeof(T) + (alignment - 1) + sizeof(void*));
    if (ptr == NULL)
        return NULL;
    void* aligned_ptr         = (void*)(((uintptr_t)ptr + sizeof(void*) + alignment - 1) & ~(alignment - 1));
    ((void**)aligned_ptr)[-1] = ptr;
    return static_cast<T*>(aligned_ptr);
}
static void aligned_free(void* aligned_ptr) { free(((void**)aligned_ptr)[-1]); }

static void full_barrier() { asm volatile("mfence" ::: "memory"); }
static void dont_optimize(const void* in) { asm volatile("" : "+m"(in)); }

#if defined(_WIN32) || defined(WIN32)
#include <windows.h>

namespace detail
{
static time_value get_frequency()
{
    LARGE_INTEGER val;
    QueryPerformanceFrequency(&val);
    return val.QuadPart;
}
}

static void set_affinity() { SetProcessAffinityMask(GetCurrentProcess(), 1); }

static void sleep(long long us) { Sleep( static_cast<DWORD>( (us + 999) / 1000) ); }

static time_value now()
{
    LARGE_INTEGER val;
    full_barrier();
    QueryPerformanceCounter(&val);
    return static_cast<time_value>(val.QuadPart);
}

static time_value frequency()
{
    static time_value freq = detail::get_frequency();
    return freq;
}

#else

#include <sys/time.h>

static time_value now()
{
    timeval val;
    full_barrier();
    gettimeofday(&val, NULL);
    return tm.tv_sec * 1000000 + tm.tv_usec;
}

static void set_affinity()
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    sched_setaffinity(getpid(), sizeof(cpuset), &cpuset);
}

static void sleep(long long us) { usleep(us); }

static time_value frequency()
{
    return 1000000; // usec
}

#endif

static tick_value tick()
{
    full_barrier();
    return __builtin_readcyclecounter();
}

static double time_between(time_value val1, time_value val2)
{
    return static_cast<double>((static_cast<long double>(val1) - static_cast<long double>(val2)) /
                               static_cast<long double>(frequency()));
}

template <typename T>
static T get_average(const std::vector<T>& measures)
{
    T sum = T();
    for (size_t i = 0; i < measures.size(); i++)
        sum += measures[i];
    return sum / measures.size();
}

template <typename T>
static T get_minimum(const std::vector<T>& measures)
{
    return *std::min_element(measures.begin(), measures.end());
}

template <typename T>
static T get_median(std::vector<T>& measures)
{
    std::sort(measures.begin(), measures.end());
    const size_t middle = measures.size() / 2;

    if (measures.size() % 2 == 1)
        return measures[middle];
    else
        return static_cast<T>((measures[middle - 1] + measures[middle]) / 2);
}

static tick_value calibrate_correction()
{
    std::vector<tick_value> values;
    for (size_t i = 0; i < 1000; i++)
    {
        const tick_value start_tick = tick();
        const tick_value stop_tick  = tick();
        values.push_back(stop_tick - start_tick);
    }
    return get_median(values);
}
