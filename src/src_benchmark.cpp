/**
 * FFT bencmarking tool (https://www.kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include "src_benchmark.hpp"
#include "json.hpp"
#include <../libs/kissfft/kissfft.hh>
#include <cinttypes>
#include <random>

#define NOMINMAX 1

bool avx2only = false;

std::string executable;

// Largest power of 2 <= n (C++17-compatible replacement for std::bit_floor)
static size_t bit_floor(size_t n)
{
    if (n == 0)
        return 0;
    size_t p = 1;
    while (p * 2 <= n)
        p *= 2;
    return p;
}

template <typename real>
static void run_accuracy_t(unsigned out_rate, unsigned in_rate, bool progress)
{
    {
        constexpr unsigned length = 9; // seconds of test signal
        src_impl_ptr<real> src(src_create<real>(out_rate, in_rate, length));
        if (!src || !src->valid)
        {
            if (progress)
                printf("  %6s  (not available)\n", type_name<real>);
            return;
        }
        std::vector<real, aligned_allocator<real>> in(in_rate * length, 0); // 8 seconds at 48 kHz
        std::vector<real, aligned_allocator<real>> out(out_rate * length);
        size_t size        = in.size() - in_rate;
        const double magn  = 0.9;
        const double scale = 3.1415926535897932384626433832795 / (4.0 * size * size * size);

        for (size_t i = 0; i < size; i++)
        {
            const double i2 = static_cast<double>(i) * static_cast<double>(i);
            in[i]           = static_cast<real>(magn * std::sin(scale * i2 * i2));
        }

        src->execute(out.data(), in.data());

        std::string filename = std::string("src_accuracy_") + executable + "_" + type_name<real> + "_" +
                               std::to_string(in_rate) + "to" + std::to_string(out_rate) + ".bin";
        FILE* f = fopen(filename.c_str(), "wb");
        if (f)
        {
            fwrite(out.data(), sizeof(real), out.size(), f);
            fclose(f);
        }
    }
    constexpr unsigned length = 2; // seconds of test signal
    src_impl_ptr<real> src(src_create<real>(out_rate, in_rate, length));
    if (!src || !src->valid)
    {
        if (progress)
            printf("  %6s  (not available)\n", type_name<real>);
        return;
    }

    size_t in_length  = static_cast<size_t>(in_rate) * length;
    size_t out_length = static_cast<size_t>(out_rate) * length;

    // Generate unit impulse at the middle of the input buffer so both pre-ring
    // and post-ring are captured regardless of the SRC's latency compensation.
    std::vector<real, aligned_allocator<real>> in(in_length, real(0));
    std::vector<real, aligned_allocator<real>> out(out_length, real(0));
    in[in_length / 2] = real(1);
    src->execute(out.data(), in.data());

    // FFT of the impulse response
    size_t fft_size = bit_floor(out_length);
    size_t half     = fft_size / 2;

    using cpx_t = std::complex<real>;
    std::vector<cpx_t> fft_in(fft_size);
    std::vector<cpx_t> fft_out(fft_size);
    for (size_t i = 0; i < fft_size; i++)
        fft_in[i] = cpx_t(out[i], real(0));

    kissfft<real> fft(fft_size, false);
    fft.transform(fft_in.data(), fft_out.data());

    // Magnitude in dB, normalized so DC = 0 dB
    double dc_mag = static_cast<double>(std::abs(fft_out[0]));
    if (dc_mag < 1e-30)
        dc_mag = 1e-30;

    std::vector<double> mag_db(half + 1);
    for (size_t i = 0; i <= half; i++)
    {
        double mag = static_cast<double>(std::abs(fft_out[i]));
        mag_db[i]  = 20.0 * std::log10(std::max(mag / dc_mag, 1e-30));
    }

    double freq_per_bin  = static_cast<double>(out_rate) / static_cast<double>(fft_size);
    double nyquist_lower = static_cast<double>(std::min(in_rate, out_rate)) / 2.0;

    // --- Passband analysis ---
    // Passband: skip DC, up to 95% of the lower Nyquist
    size_t pb_start = 1;
    size_t pb_end   = static_cast<size_t>(0.95 * nyquist_lower / freq_per_bin);
    pb_end          = std::min(pb_end, half);

    double pb_max = -1e30, pb_min = 1e30;
    for (size_t i = pb_start; i <= pb_end; i++)
    {
        pb_max = std::max(pb_max, mag_db[i]);
        pb_min = std::min(pb_min, mag_db[i]);
    }
    double passband_ripple = (pb_end >= pb_start) ? (pb_max - pb_min) : 0.0;

    // --- Cutoff frequencies (with linear interpolation) ---
    auto find_cutoff = [&](double threshold_db) -> double
    {
        for (size_t i = 1; i <= half; i++)
        {
            if (mag_db[i] < threshold_db)
            {
                // Interpolate between bin i-1 and bin i
                if (mag_db[i - 1] >= threshold_db)
                {
                    double frac = (threshold_db - mag_db[i - 1]) / (mag_db[i] - mag_db[i - 1]);
                    return (static_cast<double>(i - 1) + frac) * freq_per_bin;
                }
                return static_cast<double>(i) * freq_per_bin;
            }
        }
        return static_cast<double>(half) * freq_per_bin; // never crossed
    };

    double cutoff_1dB   = find_cutoff(-1.0);
    double cutoff_3dB   = find_cutoff(-3.0);
    double cutoff_6dB   = find_cutoff(-6.0);
    double cutoff_60dB  = find_cutoff(-60.0);
    double cutoff_120dB = find_cutoff(-120.0);

    double transition_1_60 = cutoff_60dB - cutoff_1dB;

    // --- Stopband attenuation ---
    // Only meaningful for upsampling where there's spectrum above the input Nyquist.
    // Start measuring from the -60 dB cutoff (end of transition band) or just past
    // the lower Nyquist, whichever is higher.
    double out_nyquist = static_cast<double>(out_rate) / 2.0;
    bool can_measure_stopband =
        (out_rate > in_rate) && (cutoff_60dB < out_nyquist * 0.98); // need at least 2% room
    double stopband_atten = 0.0;
    if (can_measure_stopband)
    {
        size_t sb_start = static_cast<size_t>(std::max(cutoff_60dB, nyquist_lower) / freq_per_bin) + 1;
        sb_start        = std::min(sb_start, half);
        stopband_atten  = -1000.0;
        for (size_t i = sb_start; i <= half; i++)
            stopband_atten = std::max(stopband_atten, mag_db[i]);
    }

    // --- JSON output ---
    json_open_object();
    json_key("data");
    json_string(type_name<real>);
    json_key("out_rate");
    json_number(out_rate);
    json_key("in_rate");
    json_number(in_rate);
    json_key("passband_ripple_dB");
    json_number(passband_ripple);
    json_key("cutoff_neg1dB_Hz");
    json_number(cutoff_1dB);
    json_key("cutoff_neg3dB_Hz");
    json_number(cutoff_3dB);
    json_key("cutoff_neg6dB_Hz");
    json_number(cutoff_6dB);
    json_key("cutoff_neg60dB_Hz");
    json_number(cutoff_60dB);
    json_key("cutoff_neg120dB_Hz");
    json_number(cutoff_120dB);
    json_key("transition_width_1_60_Hz");
    json_number(transition_1_60);
    if (can_measure_stopband)
    {
        json_key("stopband_attenuation_dB");
        json_number(stopband_atten);
    }

    // Decimated frequency response curve (~4096 points max)
    size_t response_count = half + 1;
    size_t step           = std::max(size_t(1), response_count / 4096);
    json_key("response_freq_step_Hz");
    json_number(freq_per_bin * static_cast<double>(step));
    json_key("response_magnitude_dB");
    json_open_array();
    for (size_t i = 0; i <= half; i += step)
        json_number(mag_db[i]);
    json_close_array();

    json_close_object();

    if (progress)
    {
        if (can_measure_stopband)
            printf(
                "  %6s  ripple %8.4f dB  -3dB %8.1f Hz  -6dB %8.1f Hz  transition %6.1f Hz atten %7.1f dB\n",
                type_name<real>, passband_ripple, cutoff_3dB, cutoff_6dB, transition_1_60, stopband_atten);
        else
            printf("  %6s  ripple %8.4f dB  -3dB %8.1f Hz  -6dB %8.1f Hz  transition %6.1f Hz atten    n/a\n",
                   type_name<real>, passband_ripple, cutoff_3dB, cutoff_6dB, transition_1_60);
    }
}

static void run_accuracy(unsigned out_rate, unsigned in_rate, bool progress)
{
    if (progress)
        printf("Accuracy SRC %u Hz -> %u Hz\n", in_rate, out_rate);
    run_accuracy_t<float>(out_rate, in_rate, progress);
    run_accuracy_t<double>(out_rate, in_rate, progress);
    if (progress)
        printf("--------------------------------------------------------------\n");
}

static void run_accuracy_twoway(unsigned out_rate, unsigned in_rate, bool progress)
{
    run_accuracy(out_rate, in_rate, progress);
    run_accuracy(in_rate, out_rate, progress);
}

template <typename real>
static void run_t(unsigned out_rate, unsigned in_rate, unsigned length, bool progress)
{
    src_impl_ptr<real> src(src_create<real>(out_rate, in_rate, length));
    if (!src || !src->valid)
    {
        if (progress)
            fprintf(stderr, "Source implementation is not valid for this configuration\n");
        return;
    }

    json_open_object();
    json_key("data");
    json_string(type_name<real>);
    json_key("out_rate");
    json_number(out_rate);
    json_key("in_rate");
    json_number(in_rate);
    json_key("length_seconds");
    json_number(length);

    // length is in seconds
    size_t in_length  = in_rate * length;
    size_t out_length = out_rate * length;
    std::vector<real, aligned_allocator<real>> in(in_length);
    std::vector<real, aligned_allocator<real>> out(out_length, real(0));

    {
        std::mt19937 rng(12345);
        std::uniform_real_distribution<real> dist(-1, 1);
        for (size_t i = 0; i < in_length; i++)
            in[i] = dist(rng);
    }

    constexpr size_t warmup_iterations = 2;
    constexpr size_t iterations        = 10;
    std::vector<double> measures;
    measures.reserve(iterations);

    for (size_t i = 0; i < warmup_iterations; i++)
    {
        src->execute(out.data(), in.data());
    }
    for (size_t i = 0; i < iterations; i++)
    {
        bench_start();
        src->execute(out.data(), in.data());
        std::chrono::nanoseconds time = bench_stop();
        measures.push_back(std::chrono::duration<double>(time).count());
    }
    auto percentiles      = get_percentiles(measures);
    double median_s       = percentiles.p50_median;
    double minimum_s      = percentiles.p1;
    double median_factor  = static_cast<double>(length) / median_s;
    double minimum_factor = static_cast<double>(length) / minimum_s;
    if (progress)
    {
        printf("%6s  %12.3f ms %5.1fx %12.3f ms %5.1fx\n", type_name<real>, median_s * 1000.0, median_factor,
               minimum_s * 1000.0, minimum_factor);
    }

    json_key("best_time");
    json_number(minimum_s * 1'000'000);
    json_key("median_time");
    json_number(median_s * 1'000'000);
    json_close_object();
}

static void run(unsigned out_rate, unsigned in_rate, unsigned length, bool progress)
{
    if (progress)
    {
        printf("Benchmarking SRC from %u Hz to %u Hz\n", in_rate, out_rate);
        printf("%6s  %15s %6s %15s %6s\n", "Type", "Median Time", "xRT", "Best Time", "xRT");
    }
    run_t<float>(out_rate, in_rate, length, progress);
    run_t<double>(out_rate, in_rate, length, progress);
    if (progress)
    {
        printf("--------------------------------------------------------------\n");
    }
}
static void run_twoway(unsigned out_rate, unsigned in_rate, unsigned length, bool progress)
{
    run(out_rate, in_rate, length, progress);
    run(in_rate, out_rate, length, progress);
}

static std::string outname;
static bool progress    = true;
static bool banner      = true;
static bool accuracy    = true;
static bool performance = true;

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    using namespace std::string_view_literals;

    executable = execfile(argv[0]);

    for (size_t i = 1; i < argc; i++)
    {
        if (argv[i] == "--save"sv)
        {
            if (i + 1 < argc)
            {
                outname = argv[i + 1];
                ++i;
            }
        }
        else if (argv[i] == "--no-accuracy"sv)
        {
            accuracy = false;
        }
        else if (argv[i] == "--accuracy"sv)
        {
            accuracy = true;
        }
        else if (argv[i] == "--no-performance"sv)
        {
            performance = false;
        }
        else if (argv[i] == "--performance"sv)
        {
            performance = true;
        }
        else if (argv[i] == "--no-progress"sv)
        {
            progress = false;
        }
        else if (argv[i] == "--progress"sv)
        {
            progress = true;
        }
        else if (argv[i] == "--no-banner"sv)
        {
            banner = false;
        }
        else if (argv[i] == "--avx2-only"sv)
        {
            avx2only = true;
        }
        else if (argv[i] == "--"sv)
        {
        }
    }

    std::string cpuname = cpu_name();
    std::string srcname = src_name();

    if (banner)
    {
        printf("DSP benchmarking tool. Copyright (C) 2016-2026 Dan Casarin https://www.kfrlib.com\n");
        printf("Benchmark source code is MIT-licensed\n");
        printf("DSP libraries have their own licenses. Please refer to the respective source code for "
               "details.\n");

        printf("CPU: %s\n", cpuname.c_str());
        printf("Algorithm: %s\n", srcname.c_str());
        printf("Compiler: %s %s\n", CMAKE_CXX_COMPILER_ID, CMAKE_CXX_COMPILER_VERSION);
    }

    if (progress)
    {
        printf("calibrating tsc...");
        fflush(stdout);
    }
    {
        benchmark_scope scope; // run at benchmarking priority/frequency
        details::calibrate_tsc();
    }

    if (progress)
    {
        printf(" %.1fMHz\n", 1000.0 / tsc_resolution());
    }

    json_open_object();

    json_key("cpu");
    json_string(cpuname);

    json_key("clock_MHz");
    json_number(1000.0 / tsc_resolution());

    json_key("library");
    json_string(srcname);

    constexpr std::pair<unsigned, unsigned> test_cases[] = {
        { 48000, 44100 }, { 96000, 48000 }, { 96000, 44100 }, { 48000, 16000 }, { 40009, 19997 },
    };

    if (performance)
    {
        json_key("performance");
        json_open_array();

        for (const auto& [out_rate, in_rate] : test_cases)
            run_twoway(out_rate, in_rate, 60, progress);

        json_close_array();
    }

    if (accuracy)
    {
        json_key("accuracy");
        json_open_array();

        for (const auto& [out_rate, in_rate] : test_cases)
            run_accuracy_twoway(out_rate, in_rate, progress);

        json_close_array();
    }

    json_close_object();

    if (outname.empty())
        return 0;
    if (outname != "-")
    {
        freopen(outname.c_str(), "w", stdout);
    }
    fputs(json_output.c_str(), stdout);

    return 0;
}