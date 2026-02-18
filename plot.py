import matplotlib.pyplot as plt
import json
import numpy as np
import math
import sys

# Function to plot the data
def plot(data, ticks, labels, title, topy, file=None):
    # Define common styles for the plot
    common_style = {
        'linestyle': '-',
        'marker': 'o',
        'markersize': 10.0,
        'markeredgewidth': 2.0,
        'markeredgecolor': '#FFFFFF'
    }

    # Define unique styles for each dataset
    styles = [
        dict(color='#F6511D', **common_style),
        dict(color='#7FB800', **common_style),
        dict(color='#00A6ED', **common_style),
        dict(color='#FFB400', **common_style),
        dict(color='#983fd3', **common_style),
        dict(color='#36cccc', **common_style),
        dict(color='#db37af', **common_style),
        dict(color='#5430e4', **common_style),
    ]

    # Define grid style
    grid_style = {'color': '#777777'}

    # Set figure size
    figsize = (14, 8)
    fig, ax = plt.subplots(figsize=figsize)

    # Enable grid with the defined style
    ax.grid(True, **grid_style)

    # Plot each dataset with its corresponding style and label
    x = None
    for d, s, l in zip(data, styles, labels):
        ax.set_xlabel('size')
        ax.set_ylabel('mflops')
        x = np.linspace(0, len(d), len(d), False)
        ax.plot(x, d, linewidth=1.6, label=l, **s)

    # Set y-axis limits
    ax.set_ylim(bottom=0.0)
    ax.set_ylim(top=topy)

    # Add legend at the bottom center
    ax.legend(loc='lower center', shadow=True)

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
        plt.savefig(file, dpi=125)

# Get input files from command-line arguments
files = sys.argv[1:]
if len(files) == 0:
    sys.exit("No input files supplied. Example: \npython plot.py data1.json data2.json … dataN.json")

print("Processing files: ", files)

# Load JSON data from the input files
results = [json.load(open(f)) for f in files]

# Extract library names and all results
libraries = [r['library'] for r in results]
all_results = [re for r in results for re in r['results']]

# Iterate over data types (float, double) and process each combination
for data in ['float', 'double']:
    for type in ['complex', 'real']:
        # Find the maximum MFLOPS value for scaling the y-axis
        mflops_max = max(
            x for x in [x.get('mflops') for x in all_results if x['data'] == data and x['type'] == type] if x is not None
        )
        topy = math.ceil(mflops_max / 10000.0) * 10000.0

        # Iterate over FFT directions and buffer types
        for direction in ['forward', 'inverse']:
            for buffer in ['inplace', 'outofplace']:
                # Generate the plot title
                title = f'{data}-{type}-{direction}-{buffer}'
                print("Generating plot: ", title)

                # Extract sizes and values for the current configuration
                sizes = [
                    x['size'] for x in results[0]['results']
                    if x['data'] == data and x['type'] == type and x['direction'] == direction and x['buffer'] == buffer
                ]
                values = [
                    [x.get('mflops') for x in r['results']
                     if x['data'] == data and x['type'] == type and x['direction'] == direction and x['buffer'] == buffer]
                    for r in results
                ]

                # Generate the plot
                plot(values, sizes, libraries, title, topy, title + '.svg')
