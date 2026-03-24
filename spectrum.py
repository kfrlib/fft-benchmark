#!/usr/bin/env python3
"""
Audio Spectrum Analyzer
Plots the magnitude frequency response of a WAV file containing an impulse response.
The impulse is assumed to reside near the centre of the audio file.
All processing is done in 64-bit (float64) for maximum precision.
"""

import argparse
import sys
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import scipy.io.wavfile as wavfile
import scipy.signal as signal
from pathlib import Path


# ---------------------------------------------------------------------------
# WAV loading  (identical approach to spectrogram.py)
# ---------------------------------------------------------------------------

def _read_wav_float64(path: str) -> tuple[int, np.ndarray]:
    """
    Load a WAV file and return (sample_rate, samples_float64).
    Samples shape: (num_samples,) mono or (num_samples, channels) stereo+.
    All sample values normalised to [-1.0, 1.0] and stored as float64.
    Handles 8/16/24/32-bit integer PCM and 32/64-bit IEEE float.
    """
    try:
        rate, data = wavfile.read(path)
    except Exception as e:
        sys.exit(f"Error reading '{path}': {e}")

    dtype = data.dtype
    if dtype == np.uint8:
        samples = (data.astype(np.float64) - 128.0) / 128.0
    elif dtype == np.int8:
        samples = data.astype(np.float64) / 128.0
    elif dtype == np.int16:
        samples = data.astype(np.float64) / 32768.0
    elif dtype == np.int32:
        samples = data.astype(np.float64) / 2147483648.0
    elif dtype == np.float32:
        samples = data.astype(np.float64)
    elif dtype == np.float64:
        samples = data
    else:
        try:
            samples = data.astype(np.float64) / np.iinfo(dtype).max
        except Exception:
            sys.exit(f"Unsupported WAV sample format: {dtype}")

    return int(rate), samples


def _to_mono_float64(samples: np.ndarray) -> np.ndarray:
    """Average all channels to produce a mono float64 signal."""
    if samples.ndim == 1:
        return samples.astype(np.float64)
    return samples.mean(axis=1).astype(np.float64)


# ---------------------------------------------------------------------------
# Window function  (identical to spectrogram.py)
# ---------------------------------------------------------------------------

def _window_function(name: str, size: int) -> np.ndarray:
    """
    Return a window of the requested size.
    Accepts any scipy.signal window name plus 'none'/'rectangular'.
    """
    lname = name.lower()
    if lname in ("none", "rect", "rectangular", "boxcar"):
        return np.ones(size, dtype=np.float64)
    try:
        win = signal.get_window(lname, size)
    except Exception:
        if lname == "kaiser":
            win = signal.get_window(("kaiser", 14), size)
        else:
            raise ValueError(
                f"Unknown window function '{name}'. "
                "Examples: hann, hamming, blackman, bartlett, kaiser, flattop, nuttall, rect."
            )
    return win.astype(np.float64)


# ---------------------------------------------------------------------------
# Spectrum computation
# ---------------------------------------------------------------------------

def compute_spectrum(
    mono: np.ndarray,
    sample_rate: int,
    fft_size: int,
    window_name: str,
) -> tuple[np.ndarray, np.ndarray]:
    """
    Compute the magnitude spectrum (dB, relative to 0 dBFS peak) of an
    impulse response.

    The impulse peak is located automatically (sample with the largest
    absolute value).  An ``fft_size``-point window is centred on that peak,
    the chosen window function is applied, and a real-valued FFT is taken.

    Returns
    -------
    freqs       : 1-D float64 array of frequency bin centres (Hz)
    magnitude_db: 1-D float64 array of magnitude values (dB, ≤ 0)
    """
    n = len(mono)

    # Locate the impulse peak
    peak_idx = int(np.argmax(np.abs(mono)))

    # Extract fft_size samples centred on the peak (zero-pad at boundaries)
    half = fft_size // 2
    start = peak_idx - half
    end   = start + fft_size          # always exactly fft_size samples

    segment = np.zeros(fft_size, dtype=np.float64)
    src_start = max(start, 0)
    src_end   = min(end, n)
    dst_start = src_start - start
    dst_end   = dst_start + (src_end - src_start)
    segment[dst_start:dst_end] = mono[src_start:src_end]

    # Apply window and compute FFT
    win      = _window_function(window_name, fft_size)
    spectrum = np.fft.rfft(segment * win, n=fft_size)
    magnitude = np.abs(spectrum)

    # Normalise to 0 dB at peak, guard against silence
    ref = np.max(magnitude)
    if ref == 0.0:
        ref = 1.0
    magnitude_db = 20.0 * np.log10(magnitude / ref + 1e-300)

    freqs = np.fft.rfftfreq(fft_size, d=1.0 / sample_rate)
    return freqs, magnitude_db


# ---------------------------------------------------------------------------
# Rendering
# ---------------------------------------------------------------------------

def render_spectrum(
    freqs: np.ndarray,
    magnitude_db: np.ndarray,
    sample_rate: int,
    dynamic_range_db: float,
    img_width: int,
    img_height: int,
    output_path: str,
    source_label: str = "",
) -> None:
    dpi = 100
    fig, ax = plt.subplots(
        figsize=(img_width / dpi, img_height / dpi), dpi=dpi
    )
    fig.patch.set_facecolor("#111111")
    ax.set_facecolor("#111111")

    # Clip to dynamic range for display
    mag_clipped = np.clip(magnitude_db, -dynamic_range_db, 0.0)

    ax.plot(freqs / 1000.0, mag_clipped, color="#00aaff", linewidth=0.8)

    # Axes limits and grid
    ax.set_xlim(freqs[0] / 1000.0, freqs[-1] / 1000.0)
    ax.set_ylim(-dynamic_range_db, 6.0)
    ax.yaxis.set_major_locator(plt.MultipleLocator(10))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(5))
    ax.xaxis.set_minor_locator(mticker.AutoMinorLocator(5))
    ax.grid(visible=True, which="major", color="#333333", linewidth=0.6)
    ax.grid(visible=True, which="minor", color="#222222", linewidth=0.3)

    # 0 dB reference line
    ax.axhline(0.0, color="#555555", linewidth=0.8, linestyle="--")

    # Styling
    for spine in ax.spines.values():
        spine.set_edgecolor("#555555")
    ax.tick_params(colors="white", labelsize=8, which="both")
    ax.xaxis.label.set_color("white")
    ax.yaxis.label.set_color("white")
    ax.set_xlabel("Frequency (kHz)", color="white", fontsize=9)
    ax.set_ylabel("Magnitude (dB)", color="white", fontsize=9)

    title = f"Spectrum  —  {source_label}" if source_label else "Spectrum"
    ax.set_title(title, color="white", fontsize=10, pad=6)

    nyquist_khz = sample_rate / 2000.0
    fft_size    = (len(freqs) - 1) * 2
    info = (
        f"FFT {fft_size}  |  Fs {sample_rate} Hz  |  "
        f"Nyquist {nyquist_khz:.1f} kHz  |  dyn {dynamic_range_db:.0f} dB"
    )
    ax.text(
        0.99, 0.02, info,
        transform=ax.transAxes,
        color="#888888", fontsize=7,
        ha="right", va="bottom",
    )

    plt.tight_layout(pad=0.6)
    fig.savefig(output_path, dpi=dpi, bbox_inches="tight", facecolor="#111111")
    plt.close(fig)
    print(f"Spectrum saved → {output_path}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description=(
            "Plot the magnitude frequency response of a WAV impulse response. "
            "The impulse is assumed to be near the centre of the file."
        ),
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("input", help="Input WAV file path")
    p.add_argument(
        "-o", "--output",
        default=None,
        help="Output image path (PNG/JPG/…). Defaults to <input>.png",
    )
    p.add_argument(
        "-d", "--dynamic-range",
        type=float,
        default=120.0,
        metavar="DB",
        help="Dynamic range in dB shown on the Y axis",
    )
    p.add_argument(
        "-n", "--fft-size",
        type=int,
        default=65536,
        metavar="N",
        help="FFT size (power-of-two recommended, e.g. 4096, 16384, 65536)",
    )
    p.add_argument(
        "-w", "--window",
        default="hann",
        metavar="NAME",
        help=(
            "Window function applied before the FFT: "
            "hann, hamming, blackman, blackmanharris, bartlett, "
            "kaiser, flattop, nuttall, rect, …"
        ),
    )
    p.add_argument(
        "-W", "--width",
        type=int,
        default=1920,
        metavar="PX",
        help="Output image width in pixels",
    )
    p.add_argument(
        "-H", "--height",
        type=int,
        default=640,
        metavar="PX",
        help="Output image height in pixels",
    )
    return p.parse_args()


def main() -> None:
    args = parse_args()

    input_path  = args.input
    output_path = args.output or (Path(input_path).stem + "_spectrum.png")

    if args.fft_size < 64:
        sys.exit("FFT size must be at least 64.")

    print(f"Loading  : {input_path}")
    sample_rate, samples = _read_wav_float64(input_path)
    mono = _to_mono_float64(samples)

    duration = len(mono) / sample_rate
    channels = 1 if samples.ndim == 1 else samples.shape[1]
    print(
        f"WAV info : {sample_rate} Hz, {channels} ch, "
        f"{len(mono)} samples ({duration:.3f} s)"
    )

    peak_idx = int(np.argmax(np.abs(mono)))
    print(
        f"Impulse  : peak at sample {peak_idx} "
        f"({peak_idx / sample_rate * 1000:.2f} ms)"
    )
    print(
        f"Settings : fft={args.fft_size}, window={args.window}, "
        f"dyn={args.dynamic_range} dB, {args.width}×{args.height} px"
    )

    freqs, magnitude_db = compute_spectrum(
        mono,
        sample_rate,
        fft_size=args.fft_size,
        window_name=args.window,
    )
    print(f"  {len(freqs)} frequency bins, resolution {freqs[1]:.4f} Hz/bin")

    render_spectrum(
        freqs,
        magnitude_db,
        sample_rate=sample_rate,
        dynamic_range_db=args.dynamic_range,
        img_width=args.width,
        img_height=args.height,
        output_path=output_path,
        source_label=Path(input_path).name,
    )


if __name__ == "__main__":
    main()
