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
import json
import random
from functools import reduce
from operator import mul

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
        plt.savefig(file, dpi=125)

# sizes = [2 ** x for x in range(4,25)]

#sizes += [x for x in range(250,301)]

# sizes = [x for x in range(4050,4097)]

#sizes = [x for x in range(16384,16384+10)]

exec_list = ['kfr','ipp','fftw'] # ,'kissfft'

def collect_data(name, sizes, type='double'):
    id = name+'_'+type+'_'+str(sizes[0])+'_'+str(sizes[-1])
    print(id, '...')
    labels = []
    mflops = []
    time   = []
    ticks  = []
    for exec_name in exec_list:
        executable = os.path.join(bin_dir, 'fft_benchmark_'+exec_name+'_'+type)
        print(executable, '...')
        output = subprocess.check_output([executable] + [str(size) for size in sizes], stderr=subprocess.STDOUT).splitlines()
        algo = [line[len(b"Algorithm: "):] for line in output if line.startswith(b"Algorithm: ")][0]
        labels.append(algo.decode("utf-8"))
        results = ast.literal_eval(b"\n".join([line[1:] for line in output if line.startswith(b">")]).decode("utf-8") )
        for result in results:
            ticks.append(result[0])
        m = [result[4] for result in results]
        t = [str(result[2]) + result[3] for result in results]
        pprint.pprint(list(zip(m, t)))
        mflops.append(m)
        time.append(t)

    with open(id+".json", "w") as json_file:
        json.dump({"mflops":mflops, "time":time, "labels":labels, "sizes":sizes}, json_file, indent=4)

    plot(mflops, ticks, labels, file=id+'.png')

primes    = [17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127]
all       = primes
powers2   = [2 ** x for x in range(4,25)]
all       += powers2
composite = [x for x in range(16, 120) if x not in all]
all       += composite

f = [2, 2, 2, 2, 2, 3, 3, 3, 5, 5, 7, 4, 8, 256, 100]
random.seed(123456)
extra = []
for i in range(0, 41):
    random.shuffle(f)
    extra += [reduce(mul, f[0:random.randint(2, len(f)//2)], 1)]

extra = [e for e in extra if e >= 100 and e <= 50000000 and e not in all]
extra = list(set(extra))
extra.sort()

collect_data('primes', primes, 'float')
collect_data('primes', primes, 'double')

collect_data('powers2', powers2, 'float')
collect_data('powers2', powers2, 'double')

collect_data('composite', composite, 'float')
collect_data('composite', composite, 'double')

collect_data('extra', extra, 'float')
collect_data('extra', extra, 'double')
