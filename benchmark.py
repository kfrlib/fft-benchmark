#!/usr/bin/env python
from __future__ import print_function

import ast
import os
import subprocess
import sys
import glob
import re
import matplotlib
import matplotlib.ticker as tck
import matplotlib.pyplot as plt
import pprint
import numpy as np

path = os.path.dirname(os.path.realpath(__file__))
bin_dir = os.path.join(path, 'bin')

def plot(data, ticks, labels, file=None):
    common_style = {'linestyle': '-', 'marker': 'o', 'markersize': 10.0, 'markeredgewidth': 2.0, 'markeredgecolor': '#FFFFFF'}
    styles = [
        dict(color = '#F6511D', **common_style),
        dict(color = '#00A6ED', **common_style),
        dict(color = '#7FB800', **common_style),
        dict(color = '#FFB400', **common_style),
        dict(color = '#0D2C54', **common_style),     
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
    legend = ax.legend(loc='lower center', shadow=True)
                
    plt.xticks(x, ticks, rotation='vertical')
    plt.tight_layout()
        
    if not file:
        plt.show()
    else:
        plt.savefig(file)

sizes = [2 ** x for x in range(10,25)]

exec_list = ['kfr','ipp','fftw','kissfft']

def collect_data(type='double'):
    labels = []
    data   = []
    ticks  = []
    for exec_name in exec_list:
        executable = os.path.join(bin_dir, 'fft_benchmark_'+exec_name+'_'+type)
        print(executable, '...')
        output = subprocess.check_output([executable] + [str(size) for size in sizes], stderr=subprocess.STDOUT).splitlines()
        algo = [line[len("Algorithm: "):] for line in output if line.startswith("Algorithm: ")][0]
        labels.append(algo)
        results = ast.literal_eval("\n".join([line[1:] for line in output if line.startswith(">")]))
        for result in results:
            ticks.append(result[0])
        results = [result[4] for result in results]
        pprint.pprint(results)
        data.append(results)
        
    return data, ticks, labels
    
plot(*collect_data())
    