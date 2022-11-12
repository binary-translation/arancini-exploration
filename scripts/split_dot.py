#!/usr/bin/env python3

import sys

_, inputFile = sys.argv

if not inputFile.endswith(".dot"):
    print(f"Input file ({inputFile}) should be a dot file.")
    exit(1)


graphs = []
g = None
with open(inputFile, "r") as fp:
    for l in fp:
        if l.startswith("digraph"):
            if g is not None:
                graphs.append(g)
            g = []
        g.append(l)
if g:
    graphs.append(g)


for i, g in enumerate(graphs):
    with open(f"{inputFile}.{i + 1}", "w") as fp:
        for l in g:
            fp.write(l)
