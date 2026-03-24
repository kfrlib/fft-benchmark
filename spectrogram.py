#!/usr/bin/env python3
"""
Audio Spectrogram Generator
Generates a spectrogram image from a WAV file, similar to ffmpeg's showspectrumpic.
Supports 8/16/24/32-bit integer and 32/64-bit float WAV files.
All processing is done in 64-bit (float64) for maximum precision.
"""

import argparse
import sys
import struct
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
from matplotlib.colors import LinearSegmentedColormap
import scipy.io.wavfile as wavfile
import scipy.signal as signal
from pathlib import Path


# ---------------------------------------------------------------------------
# Custom colour maps that mimic ffmpeg / common spectrogram palettes
# ---------------------------------------------------------------------------

def _make_colormap(name: str) -> mcolors.Colormap:
    """Return a matplotlib Colormap by name, with extras for spectrogram use."""
    builtins = plt.colormaps()

    # A few ffmpeg-inspired aliases / custom maps
    custom: dict[str, list[tuple]] = {
        "intensity": [
            (0.0,  (0.00, 0.00, 0.00)),
            (0.25, (0.45, 0.00, 0.60)),
            (0.50, (0.90, 0.20, 0.00)),
            (0.75, (1.00, 0.80, 0.00)),
            (1.0,  (1.00, 1.00, 1.00)),
        ],
        "sox": [
            (0.0,  (0.00, 0.00, 0.00)),
            (0.17, (0.10, 0.10, 0.60)),
            (0.33, (0.00, 0.50, 0.90)),
            (0.50, (0.00, 0.80, 0.00)),
            (0.67, (0.90, 0.90, 0.00)),
            (0.83, (0.90, 0.30, 0.00)),
            (1.0,  (1.00, 1.00, 1.00)),
        ],
        "fiery": [
            (0.0,  (0.00, 0.00, 0.00)),
            (0.33, (0.50, 0.00, 0.00)),
            (0.66, (1.00, 0.50, 0.00)),
            (1.0,  (1.00, 1.00, 0.80)),
        ],
    }

    lname = name.lower()
    if lname in custom:
        stops = custom[lname]
        cmap = LinearSegmentedColormap.from_list(
            lname, [(t, rgb) for t, rgb in stops]
        )
        return cmap

    # Fall back to any matplotlib map (case-insensitive search)
    for candidate in builtins:
        if candidate.lower() == lname:
            return plt.get_cmap(candidate)

    raise ValueError(
        f"Unknown colour map '{name}'. "
        f"Built-ins: {sorted(builtins)}. "
        f"Extra maps: {sorted(custom.keys())}."
    )


# ---------------------------------------------------------------------------
# WAV loading (handles int8/16/24/32 and float32/64)
# ---------------------------------------------------------------------------

def _read_wav_float64(path: str) -> tuple[int, np.ndarray]:
    """
    Load a WAV file and return (sample_rate, samples_float64).
    samples shape: (num_samples,) for mono, (num_samples, num_channels) for stereo+.
    All sample values are normalised to [-1.0, 1.0] and stored as float64.
    Handles 8/16/24/32-bit integer PCM and 32/64-bit IEEE float.
    """
    try:
        rate, data = wavfile.read(path)
    except Exception as e:
        sys.exit(f"Error reading '{path}': {e}")

    dtype = data.dtype

    if dtype == np.int8:
        samples = data.astype(np.float64) / 128.0
    elif dtype == np.int16:
        samples = data.astype(np.float64) / 32768.0
    elif dtype == np.int32:
        samples = data.astype(np.float64) / 2147483648.0
    elif dtype == np.float32:
        samples = data.astype(np.float64)
    elif dtype == np.float64:
        samples = data  # already float64
    elif dtype == np.uint8:                   # 8-bit WAV is unsigned
        samples = (data.astype(np.float64) - 128.0) / 128.0
    else:
        # 24-bit PCM arrives as int32 from scipy; catch any remaining edge cases
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
# Spectrogram computation
# ---------------------------------------------------------------------------

def _window_function(name: str, size: int) -> np.ndarray:
    """
    Return a window of the requested size.
    Accepts any scipy.signal window name (hann, hamming, blackman, kaiser, etc.)
    plus 'none'/'rectangular' for a rectangular (flat) window.
    """
    lname = name.lower()
    if lname in ("none", "rect", "rectangular", "boxcar"):
        return np.ones(size, dtype=np.float64)

    # scipy.signal.get_window accepts strings like 'kaiser' but kaiser needs a
    # beta parameter.  Provide a sensible default (14) when the user just says "kaiser".
    try:
        win = signal.get_window(lname, size)
    except Exception:
        # Try with beta for kaiser
        if lname == "kaiser":
            win = signal.get_window(("kaiser", 14), size)
        else:
            raise ValueError(
                f"Unknown window function '{name}'. "
                "Examples: hann, hamming, blackman, bartlett, kaiser, flattop, nuttall, rect."
            )
    return win.astype(np.float64)


def compute_spectrogram(
    mono: np.ndarray,
    sample_rate: int,
    fft_size: int,
    window_name: str,
    dynamic_range_db: float,
    img_width: int,
    img_height: int,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """
    Compute a power spectrogram suitable for image display.

    Returns
    -------
    spec_db : 2-D float64 array, shape (freq_bins, time_frames)
              values clipped to [−dynamic_range_db, 0] dB
    freqs   : 1-D array of frequency bin centres (Hz)
    times   : 1-D array of frame centre times (s)
    """
    win = _window_function(window_name, fft_size)
    hop = fft_size // 8

    # scipy.signal.spectrogram returns (freqs, times, Sxx)
    freqs, times, Sxx = signal.spectrogram(
        mono,
        fs=sample_rate,
        window=win,
        nperseg=fft_size,
        noverlap=fft_size - hop,
        nfft=fft_size,
        scaling="spectrum",
        mode="psd",
        detrend=False,
    )

    # Convert to dB, avoid log(0)
    ref = np.max(Sxx)
    if ref == 0.0:
        ref = 1.0
    spec_db = 10.0 * np.log10(Sxx / ref + 1e-300)
    spec_db = np.clip(spec_db, -dynamic_range_db, 0.0)

    return spec_db, freqs, times


# ---------------------------------------------------------------------------
# Rendering
# ---------------------------------------------------------------------------

def render_spectrogram(
    spec_db: np.ndarray,
    freqs: np.ndarray,
    times: np.ndarray,
    sample_rate: int,
    dynamic_range_db: float,
    color_scheme: str,
    img_width: int,
    img_height: int,
    output_path: str,
    source_label: str = "",
) -> None:
    dpi = 100
    fig_w = img_width / dpi
    fig_h = img_height / dpi

    fig, ax = plt.subplots(figsize=(fig_w, fig_h), dpi=dpi)
    fig.patch.set_facecolor("black")
    ax.set_facecolor("black")

    cmap = _make_colormap(color_scheme)

    # extent: [x_left, x_right, y_bottom, y_top]
    extent = [times[0], times[-1], freqs[0] / 1000.0, freqs[-1] / 1000.0]

    im = ax.imshow(
        spec_db,
        origin="lower",
        aspect="auto",
        extent=extent,
        cmap=cmap,
        vmin=-dynamic_range_db,
        vmax=0.0,
        interpolation="bilinear",
    )

    # Axes styling
    for spine in ax.spines.values():
        spine.set_edgecolor("#555555")

    ax.tick_params(colors="white", labelsize=8)
    ax.xaxis.label.set_color("white")
    ax.yaxis.label.set_color("white")
    ax.set_xlabel("Time (s)", color="white", fontsize=9)
    ax.set_ylabel("Frequency (kHz)", color="white", fontsize=9)

    title = f"Spectrogram  —  {source_label}" if source_label else "Spectrogram"
    ax.set_title(title, color="white", fontsize=10, pad=6)

    # Colour bar
    cbar = fig.colorbar(im, ax=ax, pad=0.02, fraction=0.03)
    cbar.set_label("Power (dB)", color="white", fontsize=8)
    cbar.ax.yaxis.set_tick_params(color="white", labelsize=7)
    plt.setp(cbar.ax.yaxis.get_ticklabels(), color="white")
    cbar.outline.set_edgecolor("#555555")

    plt.tight_layout(pad=0.5)
    fig.savefig(output_path, dpi=dpi, bbox_inches="tight", facecolor="black")
    plt.close(fig)
    print(f"Spectrogram saved → {output_path}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Generate an audio spectrogram image from a WAV file.",
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
        help="Dynamic range in dB (higher = more detail in quiet parts)",
    )
    p.add_argument(
        "-c", "--color",
        default="inferno",
        metavar="NAME",
        help=(
            "Colour map name. Built-in extras: intensity, sox, fiery. "
            "Any matplotlib map also works (inferno, magma, plasma, viridis, …)."
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
    p.add_argument(
        "-w", "--window",
        default="hann",
        metavar="NAME",
        help=(
            "Window function: hann, hamming, blackman, bartlett, "
            "kaiser, flattop, nuttall, rect, …"
        ),
    )
    p.add_argument(
        "-n", "--fft-size",
        type=int,
        default=4096,
        metavar="N",
        help="FFT size (power-of-two recommended, e.g. 1024, 2048, 4096, 8192)",
    )
    return p.parse_args()


def main() -> None:
    args = parse_args()

    input_path = args.input
    output_path = args.output or (Path(input_path).stem + ".png")

    # Validate FFT size
    if args.fft_size < 64:
        sys.exit("FFT size must be at least 64.")

    print(f"Loading  : {input_path}")
    sample_rate, samples = _read_wav_float64(input_path)
    mono = _to_mono_float64(samples)

    duration = len(mono) / sample_rate
    channels = 1 if samples.ndim == 1 else samples.shape[1]
    # print(
    #     f"WAV info : {sample_rate} Hz, {channels} ch, "
    #     f"{duration:.2f} s, dtype={samples.dtype}"
    # )
    # print(
    #     f"Settings : fft={args.fft_size}, window={args.window}, "
    #     f"dyn={args.dynamic_range} dB, color={args.color}, "
    #     f"{args.width}×{args.height} px"
    # )

    # print("Computing spectrogram …")
    spec_db, freqs, times = compute_spectrogram(
        mono,
        sample_rate,
        fft_size=args.fft_size,
        window_name=args.window,
        dynamic_range_db=args.dynamic_range,
        img_width=args.width,
        img_height=args.height,
    )
    print(
        f"  {spec_db.shape[0]} freq bins × {spec_db.shape[1]} time frames"
    )

    # print("Rendering …")
    render_spectrogram(
        spec_db,
        freqs,
        times,
        sample_rate=sample_rate,
        dynamic_range_db=args.dynamic_range,
        color_scheme=args.color,
        img_width=args.width,
        img_height=args.height,
        output_path=output_path,
        source_label=Path(input_path).name,
    )


if __name__ == "__main__":
    main()
