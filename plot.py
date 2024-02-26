import matplotlib.pyplot as plt
import json
import numpy as np
import math
import sys

def plot(data, ticks, labels, title, topy, file=None):
    common_style = {'linestyle': '-', 'marker': 'o', 'markersize': 10.0, 'markeredgewidth': 2.0, 'markeredgecolor': '#FFFFFF'}
    styles = [
        dict(color = '#F6511D', **common_style),
        dict(color = '#00A6ED', **common_style),
        dict(color = '#7FB800', **common_style),
        dict(color = '#FFB400', **common_style),
        dict(color = '#983fd3', **common_style),
        dict(color = '#36cccc', **common_style),
        dict(color = '#db37af', **common_style),
        dict(color = '#5430e4', **common_style),
        ]
    grid_style = {'color': '#777777'}
    figsize = (14, 8)
    fig, ax = plt.subplots(figsize=figsize)
    ax.grid(True, **grid_style)
    for d, s, l in zip(data, styles, labels):
        ax.set_xlabel('size')
        ax.set_ylabel('mflops')
        x = np.linspace(0,len(d),len(d), False)
        ax.plot(x, d, linewidth=1.6, label=l, **s)

    ax.set_ylim(bottom=0.0)
    ax.set_ylim(top=topy)
    ax.legend(loc='lower center', shadow=True)

    ticks = ticks[:len(x)]

    plt.title(title)
    plt.xticks(x, ticks, rotation='vertical')
    plt.tight_layout()

    if not file:
        plt.show()
    else:
        plt.savefig(file, dpi=125)

print(sys.argv)
files = sys.argv[1:]
if len(files) == 0:
    sys.exit("No input files supplied. Example: \npython plot.py data1.json data2.json … dataN.json")

print("Processing files: ", files)
results      = [json.load(open(f)) for f in files]
libraries    = [r['library'] for r in results]
all_results  = [re for r in results for re in r['results']]

mflops_f     = [x.get('mflops') for x in all_results if x['data']=='float']
mflops_d     = [x.get('mflops') for x in all_results if x['data']=='double']
mflops_f_max = max(x for x in mflops_f if x is not None)
mflops_d_max = max(x for x in mflops_d if x is not None)
mflops_f_max = math.ceil(mflops_f_max/10000.0)*10000.0
mflops_d_max = math.ceil(mflops_d_max/10000.0)*10000.0

for data in ['float', 'double']:
    topy = mflops_f_max if data=='float' else mflops_d_max
    for type in ['complex', 'real']:
        for direction in ['forward', 'inverse']:
            for buffer in ['inplace', 'outofplace']:
                title     = f'{data}-{type}-{direction}-{buffer}'
                print("Generating plot: ", title)
                sizes     = [x['size'] for x in results[0]['results'] if x['data']==data and x['type']==type and x['direction']==direction and x['buffer']==buffer]
                values    = [ ([x.get('mflops') for x in r['results'] if x['data']==data and x['type']==type and x['direction']==direction and x['buffer']==buffer]) for r in results ]                
                plot(values, sizes, libraries, title, topy, title+'.svg')
