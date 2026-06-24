import matplotlib.pyplot as plt
from matplotlib.ticker import MultipleLocator
import json
import numpy as np
import math
import sys

common_style = {
    'linestyle': '-',
    'markersize': 8.0,
    'markeredgewidth': 1.25,
    'markeredgecolor': '#FFFFFF',
    'linewidth': 1.25,
}

# Define unique markers for each dataset
markers = [
    'o', 'D', 's'
]

# Define unique styles for each dataset
styles = [
    dict(color="#A8021D", marker=markers[0], **common_style),
    dict(color='#36cccc', marker=markers[2], **common_style),
    dict(color='#00A6ED', marker=markers[1], **common_style),
    dict(color='#FFB400', marker=markers[0], **common_style),
    # dict(color="#FF8964", marker=markers[2], **common_style),
    dict(color="#EF3608", marker=markers[1], **common_style),
    dict(color='#e9c46a', marker=markers[0], **common_style),
    dict(color='#983fd3', marker=markers[1], **common_style),
    dict(color='#db37af', marker=markers[2], **common_style),
    dict(color='#5430e4', marker=markers[0], **common_style),
    dict(color='#7FB800', marker=markers[1], **common_style),
    dict(color="#d4388b", marker=markers[2], **common_style),
    dict(color='#2a9d8f', marker=markers[0], **common_style),
]

# Function to plot the data
def plot(data, ticks, labels, title, topy, file=None):
    # Define common styles for the plot

    # Define grid style
    grid_style = {'color': '#777777'}

    # Set figure size
    figsize = (14, 8)
    fig, ax = plt.subplots(figsize=figsize)

    # Enable grid with the defined style
    ax.grid(True, **grid_style)

    # Add thin dotted minor grid lines at every gigaflop
    ax.yaxis.set_minor_locator(MultipleLocator(1))
    ax.grid(True, which='minor', axis='y', color='#888888',
            linestyle=':', linewidth=0.5, zorder=0)

    # Plot each dataset with its corresponding style and label.
    # Datasets with no data are skipped (so they don't appear in the
    # legend), but the style index is kept in sync with the dataset
    # index so colors are not reassigned to other libraries.
    x = None
    for i, (d, s, l) in enumerate(zip(data, styles, labels)):
        ax.set_xlabel('FFT size')
        ax.set_ylabel('GFlops')
        # Skip datasets that have no data points (empty list or all-None),
        # so their library name does not show up in the legend.
        if not d or all(v is None for v in d):
            continue
        x = np.linspace(0, len(d), len(d), False)
        ax.plot(x, d, label=(l[:26] + '…') if len(l) > 27 else l, **s)

    # Set y-axis limits
    ax.set_ylim(bottom=0.0)
    ax.set_ylim(top=topy)

    # Add legend at the bottom center
    ax.legend(loc='upper right', shadow=True)

    # Adjust x-axis ticks
    if x is not None:  # Ensure x is defined before using it
        ticks = ticks[:len(x)]
        plt.xticks(x, ticks, rotation='vertical')
    plt.title(title)
    plt.tight_layout()

    # Save the plot to a file or display it
    if not file:
        plt.show()
    else:
        plt.savefig(file, dpi=192)

# Get input files from command-line arguments
files = sys.argv[1:]
if len(files) == 0:
    sys.exit("No input files supplied. Example: \npython plot.py data1.json data2.json … dataN.json")

print("Processing files: ", files)

# Load JSON data from the input files
results = [json.load(open(f)) for f in files]

# Ensure "cpu" is the same across all passed .json files
cpus = {r.get('cpu') for r in results}
if len(cpus) > 1:
    sys.exit("Inconsistent 'cpu' fields across input files: " + ", ".join(repr(c) for c in cpus))

# Merge files with the same "library". When multiple values are provided for
# the same set of params (size, data, type, direction, buffer), compute the
# median of those values.
from collections import defaultdict

# Group results by library name, sorted alphabetically so the same color
# is assigned to a given library regardless of the .json argument order.
merged_results = []
groups = defaultdict(list)
for r in results:
    groups[r['library']].append(r)

for lib in sorted(groups, key=str.lower):
    group = groups[lib]
    # Bucket performance entries by (size, data, type, direction, buffer)
    buckets = defaultdict(list)
    for r in group:
        for entry in r['performance']:
            key = (entry['size'], entry['data'], entry['type'],
                   entry['direction'], entry['buffer'])
            buckets[key].append(entry)

    merged_performance = []
    for key, entries in buckets.items():
        # Pick a representative entry (first one) and replace gflops/best_time/
        # median_time with the median across the merged entries.
        merged = dict(entries[0])
        for field in ('gflops', 'best_time', 'median_time'):
            vals = [e[field] for e in entries if field in e]
            if vals:
                merged[field] = float(np.median(vals))
        merged_performance.append(merged)

    # Sort by size for deterministic output
    merged_performance.sort(key=lambda e: (e['size'], e['data'], e['type'],
                                           e['direction'], e['buffer']))

    merged_results.append({
        'cpu': group[0].get('cpu'),
        'clock_MHz': group[0].get('clock_MHz'),
        'library': lib,
        'accuracy': [a for r in group for a in r.get('accuracy', [])],
        'performance': merged_performance,
    })

results = merged_results

# Extract library names and all results
libraries = [r['library'] for r in results]
all_results = [re for r in results for re in r['performance']]

# Iterate over data types (float, double) and process each combination
for data in ['float', 'double']:
    for type in ['complex', 'real']:
        try:
            print(f"Processing data type: {data}, type: {type}")
            filtered_results = [x for x in all_results if x['data'] == data and x['type'] == type]

            # Find the maximum GFLOPS value for scaling the y-axis
            gflops_max = max(
                x for x in [x.get('gflops') for x in filtered_results] if x is not None
            )
            topy = math.ceil(gflops_max / 10.0) * 10.0

            # Iterate over FFT directions and buffer types
            for direction in ['forward', 'inverse']:
                for buffer in ['inplace', 'outofplace']:
                    # Generate the plot title
                    title = f'{data.capitalize()} {type.capitalize()} {direction.capitalize()} {"In-place" if buffer == "inplace" else "Out-of-place"} FFT'

                    # Extract sizes and values for the current configuration
                    sizes = [
                        x['size'] for x in results[0]['performance']
                        if x['data'] == data and x['type'] == type and x['direction'] == direction and x['buffer'] == buffer
                    ]
                    values = [
                        [x.get('gflops') for x in r['performance']
                        if x['data'] == data and x['type'] == type and x['direction'] == direction and x['buffer'] == buffer]
                        for r in results
                    ]

                    # Skip plot generation if no data was collected for these parameters
                    if not sizes or not any(values):
                        print(f"Skipping plot (no data): ", title)
                        continue

                    print("Generating plot: ", title)

                    # Generate the plot
                    plot(values, sizes, libraries, title, topy, 'plots/' + title + '.svg')
        except Exception as e:
            print(f"Error processing {data}-{type}: {e}")            
