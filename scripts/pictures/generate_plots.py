#!/usr/bin/env python3
"""
Generates all paper plots. Use --bw to produce black-and-white PDFs with a -bw suffix.
Run from scripts/pictures/.
"""

import argparse
import os
import pandas as pd
import matplotlib as mpl
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

parser = argparse.ArgumentParser(description="Generate paper plots")
parser.add_argument("--bw", action="store_true", help="Generate black-and-white plots with -bw suffix")
args = parser.parse_args()

BW = args.bw
SUFFIX = "-bw" if BW else ""
FMT = "pdf"

# Palettes
if BW:
    PALETTE_MAIN = "Greys"
    PALETTE_LINES = ["black", "black", "black"]
    PALETTE_BARS = "Greys"
    REF_LINE_COLOR = "black"
    OVERFLOW_TEXT_COLOR = "black"
else:
    cp = sns.color_palette("muted")
    PALETTE_MAIN = cp
    PALETTE_LINES = "colorblind"
    PALETTE_BARS = "colorblind"
    REF_LINE_COLOR = sns.color_palette(palette="dark")[3]
    OVERFLOW_TEXT_COLOR = None  # use bar color

os.chdir(os.path.dirname(os.path.realpath(__file__)))

if BW:
    mpl.rcParams['hatch.linewidth'] = 0.5
    mpl.rcParams['hatch.color'] = 'black'

# ── Helpers ──────────────────────────────────────────────────────────────────

bm = {"native":99, "arancini":1, "arancini+nlib":2, "lasagne":98,
      "arancini-no-fm":3, "arancini-no-fm+nlib":4, "risotto":4,
      "risotto+nlib":5, "arancini-no-df":3, "arancini-no-df+nlib":4}
sm = {"hi":"hi", "km":"km", "li":"lr", "ma":"mm", "pc":"pc", "st":"sm", "wo":"wc"}
bm_dist = {"hi":0, "lr":2, "mm":3, "pc":4, "sm":5, "km":1, "wc":6}
hatches = ["//////", "......", "", "\\\\\\\\\\\\", "xxxxxx", "OOOOOO", "++++"]

def to_idx(s):
    if s.name != "emulator":
        return s
    return s.apply(lambda b: bm[b])

def to_idx_dist(s):
    return s.apply(lambda b: bm_dist[b])

def gmean(x):
    return np.exp(np.mean(np.log(x)))

def to_var(s, ver):
    return "pthread" if "-pthread" in s else "map-reduce"

def to_short(s):
    return sm[s]

def normalize(s, e, c):
    tmp = s
    base = s[s[c] == e]
    tmp[f'normalized_time_{e}'] = tmp["time"].map(lambda l: l / base["time"].median())
    return tmp

def inv_normalize(s, e, c):
    tmp = s
    base = s[s[c] == e]
    tmp[f'normalized_time_{e}'] = tmp[["time", "threads"]].apply(
        lambda l: base["time"].median() / (l["time"] * l["threads"]), axis=1)
    return tmp

# Bar styles for vertical plots: (fill, hatch)
BAR_STYLES = [
    ("black", ""),            # solid black
    ("white", "//////"),      # forward slash
    ("white", ""),            # solid white
    ("white", "\\\\\\\\\\\\"),  # backward slash
]

def apply_hatches(axs, grid_legend):
    bars = []
    for ax in axs:
        for p in ax.patches:
            bars.append(p)
    handles = grid_legend.get_patches()
    hmap = {}
    i = 0
    for bar in bars:
        fc = bar.get_facecolor()
        if fc not in hmap:
            hmap[fc] = BAR_STYLES[i] if i < len(BAR_STYLES) else ("white", hatches[i])
            i += 1
        bar.set_hatch(hmap[fc][1])
    for h in handles:
        fc = h.get_facecolor()
        if fc in hmap:
            h.set_hatch(hmap[fc][1])
    if BW:
        for bar in bars:
            style = hmap.get(bar.get_facecolor(), ("white", ""))
            bar.set_facecolor(style[0])
            bar.set_edgecolor("black")
        for h in handles:
            style = hmap.get(h.get_facecolor(), ("white", ""))
            h.set_facecolor(style[0])
            h.set_edgecolor("black")
    return bars, hmap

def savefig(fig_or_grid, name):
    outpath = f"{name}{SUFFIX}.{FMT}"
    if hasattr(fig_or_grid, 'savefig'):
        fig_or_grid.savefig(outpath, bbox_inches="tight")
    # Also save PNG for visual checking
    pngpath = f"{name}{SUFFIX}.png"
    if hasattr(fig_or_grid, 'savefig'):
        fig_or_grid.savefig(pngpath, bbox_inches="tight", dpi=200)
    print(f"  Saved {outpath} + {pngpath}")

# ── Load data ────────────────────────────────────────────────────────────────

print("Loading data...")

opt = []
scale = []

# Arm data
append = []
for f in ["../../bench/aarch64-opt/times.csv",
          "../../bench/aarch64-noopt/times.csv",
          "../../bench/aarch64-wc/times.csv",
          "../../bench/aarch64-wc2/times.csv",
          "../../bench/aarch64-wc3/times.csv",
          "../../bench/aarch64-2024-11-26_11:08:10/times.csv"]:
    append.append(pd.read_csv(f))
base = pd.concat(append, ignore_index=True)
base["version"] = "Arm"
base.drop(base[base["time"] == -1.0].index, inplace=True)
base.drop(base[base["emulator"] == "qemu"].index, inplace=True)
base.drop(base[base["emulator"] == "risotto-nofence"].index, inplace=True)
opt.append(base.copy())
scale.append(base.copy())

# RISC-V data
append = []
for f in ["../../bench/riscv64-all/times.csv",
          "../../bench/riscv64-append/times.csv",
          "../../bench/riscv64-append2/times.csv",
          "../../bench/riscv64-append3/times.csv",
          "../../bench/riscv64-noopt/times.csv",
          "../../bench/riscv64-wc/times.csv",
          "../../bench/riscv64-2024-11-26_10:02:34/times.csv",
          "../../bench/riscv64-wc-full/times.csv"]:
    append.append(pd.read_csv(f))
base = pd.concat(append, ignore_index=True)
base["version"] = "RISC-V"
base.drop(base[base["time"] == -1.0].index, inplace=True)
base.drop(base[base["emulator"] == "qemu"].index, inplace=True)
base.drop(base[base["emulator"] == "risotto-nofence"].index, inplace=True)
opt.append(base.copy())
scale.append(base.copy())

# Prepare opt
for i in [0, 1]:
    opt[i].drop(opt[i][opt[i]["emulator"] == "risotto"].index, inplace=True)
    opt[i].drop(opt[i][opt[i]["emulator"] == "risotto+nlib"].index, inplace=True)
    opt[i].drop(opt[i][opt[i]["emulator"] == "lasagne"].index, inplace=True)
    opt[i]["short"] = opt[i]["benchmark"].apply(lambda l: to_short(l[0:2]))
    arch = "Arm" if i == 0 else "RISC-V"
    opt[i]["variant"] = opt[i]["benchmark"].apply(to_var, ver=arch)
    opt[i].sort_values(by=["emulator", "version", "benchmark"], key=to_idx, inplace=True)

# Prepare scale
for i in [0, 1]:
    for e in ["arancini-no-df", "arancini-no-df+nlib", "arancini-no-fm", "arancini-no-fm+nlib"]:
        scale[i].drop(scale[i][scale[i]["emulator"] == e].index, inplace=True)
    scale[i] = scale[i].groupby(["benchmark", "threads"], as_index=False)[scale[i].columns].apply(normalize, "native", "emulator")
    scale[i] = scale[i].groupby(["benchmark", "emulator"], as_index=False)[scale[i].columns].apply(inv_normalize, 1, "threads")
    scale[i]["short"] = scale[i]["benchmark"].apply(lambda l: to_short(l[0:2]))
    arch = "Arm" if i == 0 else "RISC-V"
    scale[i]["variant"] = scale[i]["benchmark"].apply(to_var, ver=arch)
    scale[i].sort_values(by=["emulator", "benchmark"], key=to_idx, inplace=True)

data = pd.concat(scale, ignore_index=True)
data2 = data.copy()
data.drop(data[data["emulator"] == "native"].index, inplace=True)

# ── Plot 1: phoenix ──────────────────────────────────────────────────────────

print("Generating phoenix...")

mpl.rcParams["pdf.fonttype"] = 42
mpl.rcParams["ps.fonttype"] = 42
mpl.rcParams["figure.labelsize"] = 8

sns.set_style("ticks", {"xtick.major.size": 8, "ytick.major.size": 8, "axes.grid": True})
sns.set_context("paper", rc={"font.size": 8, "axes.titlesize": 8, "axes.labelsize": 8})

dat = data.copy()
dat.drop(dat[dat["emulator"] == "arancini"].index, inplace=True)
dat.drop(dat[dat["emulator"] == "risotto"].index, inplace=True)

dat["emulator"] = dat["emulator"].apply(lambda l: "Arancini" if l == "arancini+nlib" else l)
dat["emulator"] = dat["emulator"].apply(lambda l: "Risotto" if l == "risotto+nlib" else l)
dat["emulator"] = dat["emulator"].apply(lambda l: "Lasagne" if l == "lasagne" else l)

BAR_WIDTH = 0.65
BAR_EDGE = "black" if BW else None

grid = sns.catplot(data=dat[dat["threads"] == 16], row="version", col="variant", kind="bar",
                   height=1.2, aspect=1.6, x="short", y="normalized_time_native", hue="emulator",
                   palette=PALETTE_MAIN, estimator=gmean, width=BAR_WIDTH, edgecolor=BAR_EDGE)
grid.set_axis_labels("", "")
# Clear all titles, then set col titles on first row only
for ax in grid.axes.flatten():
    ax.set_title("")
for ax in grid.axes[0]:
    col_idx = list(grid.axes[0]).index(ax)
    ax.set_title(grid.col_names[col_idx], fontsize=8)
# Row titles on the right
for i, row_val in enumerate(grid.row_names):
    grid.axes[i, -1].annotate(row_val, xy=(1.05, 0.5), xycoords="axes fraction",
                               fontsize=8, ha="left", va="center", rotation=270)

fig = grid.figure
fig.supylabel("Runtime relative to native")
axs = grid.axes.flatten()
for a in axs:
    a.set_ylim(0, 20)
grid.set(yticks=np.arange(5, 20, 5))

bars, _ = apply_hatches(axs, grid.legend)

for bar in bars:
    if bar.get_height() >= 22:
        bar.axes.text(
            bar.get_x() + bar.get_width() / 2.0, 20.3,
            str(int(bar.get_height())), fontsize=5,
            ha="center", va="bottom", rotation=90, color="black",
            zorder=10, clip_on=False)

leg = grid.legend
leg.remove()
grid.add_legend(loc="lower center", title=None, fontsize=8,
                bbox_to_anchor=(0.5, 0.975), ncol=5, frameon=False)

fig.set_size_inches((5.91, 3.5))
fig.tight_layout()
savefig(plt, "phoenix")
plt.close("all")

# ── Plot 2: scaling ──────────────────────────────────────────────────────────

print("Generating scaling...")

sns.set_style("ticks", {"xtick.major.size": 8, "ytick.major.size": 8, "axes.grid": True})
sns.set_context("paper", rc={"font.size": 8, "axes.titlesize": 8, "axes.labelsize": 8})

out = ["risotto", "risotto+nlib", "lasagne", "arancini+nlib"]
for o in out:
    data2.drop(data2[data2["emulator"] == o].index, inplace=True)

data2["emulator"] = data2["emulator"].apply(lambda l: "Arancini" if l == "arancini" else l)
data2["emulator"] = data2["emulator"].apply(lambda l: "Native" if l == "native" else l)

BW_DASHES = [(1,0), (5,3)]
grid = sns.FacetGrid(data=data2, row="version", col="short", height=1.2, aspect=0.85,
                     margin_titles=True, gridspec_kws={"wspace": 0.15, "hspace": 0.35})
grid.map_dataframe(sns.lineplot, x="threads", y="normalized_time_1", hue="emulator",
                   palette=PALETTE_LINES, estimator=gmean, errorbar=None,
                   style="emulator", markers=False, dashes=BW_DASHES, linewidth=1.0)
grid.set_axis_labels("", "")
# Only show col titles on first row
grid.set_titles(col_template="", row_template="")
for ax in grid.axes[0]:
    col_idx = list(grid.axes[0]).index(ax)
    ax.set_title(grid.col_names[col_idx], fontsize=8)

# Row titles on the right
for i, (row_val, ax_row) in enumerate(zip(grid.row_names, grid.axes)):
    ax_row[-1].annotate(row_val, xy=(1.15, 0.5), xycoords="axes fraction",
                        fontsize=8, ha="left", va="center", rotation=270)
grid.set(xscale="log", xticks=[1, 2, 8, 64], yticks=[0.5, 1])
for a in grid.axes.flatten():
    a.set_xticklabels(["1", "2", "8", "64"])
fig = grid.figure

# deduplicate legend entries
handles, labels = grid.axes.flatten()[0].get_legend_handles_labels()
seen = {}
unique_h, unique_l = [], []
for h, l in zip(handles, labels):
    if l not in seen:
        seen[l] = True
        unique_h.append(h)
        unique_l.append(l)

fig.supylabel("Relative scaling factor", x=0.01)
fig.supxlabel("#Threads")
fig.set_size_inches((5.91, 3.0))
fig.subplots_adjust(top=0.82, bottom=0.15, left=0.08, hspace=0.4)
fig.legend(unique_h, unique_l, loc="upper center", title=None, fontsize=8,
           bbox_to_anchor=(0.5, 1.0), ncol=3, frameon=False)
savefig(grid, "scaling_full")
plt.close("all")

# ── Plot 3: opt-full ─────────────────────────────────────────────────────────

print("Generating opt-full...")

sns.set_style("ticks", {"xtick.major.size": 8, "ytick.major.size": 8, "axes.grid": True})
sns.set_context("paper", rc={"font.size": 8, "axes.titlesize": 8, "axes.labelsize": 8})

dota = pd.concat(opt, ignore_index=True)
fm = dota.copy()

fm.drop(fm[fm["emulator"] == "native"].index, inplace=True)
fm.drop(fm[fm["emulator"] == "arancini-no-df+nlib"].index, inplace=True)
fm.drop(fm[fm["emulator"] == "arancini-no-fm+nlib"].index, inplace=True)
fm.drop(fm[fm["emulator"] == "arancini+nlib"].index, inplace=True)

def to_opt(e):
    if "df" in e: return "df"
    if "fm" in e: return "fm"
    return ""

def adj_name(e):
    if "df" in e: return "Arancini w/o opt"
    if "fm" in e: return "Arancini w/o opt"
    return "Arancini w/ opt"

fm["name"] = fm[["emulator", "version"]].apply(
    lambda l: f'{adj_name(l["emulator"])} - {l["version"]}', axis=1)
fm["opt"] = fm["emulator"].apply(to_opt)

based = fm[fm["opt"] == ""]
b1 = based.copy(); b1["opt"] = "df"
b2 = based.copy(); b2["opt"] = "fm"
fm.drop(based.index, inplace=True)
fm = pd.concat([fm, b1, b2], ignore_index=True)

fm = fm.groupby(["benchmark", "threads", "version", "variant"], as_index=False)[fm.columns].apply(normalize, "arancini", "emulator")
fm.sort_values(by=["emulator", "benchmark"], key=to_idx, inplace=True)
fm.drop(fm[fm["emulator"] == "arancini"].index, inplace=True)

grid = sns.catplot(data=fm, row="opt", col="variant", kind="bar", height=1.2, aspect=1.6,
                   x="short", y="normalized_time_arancini", hue="name", palette=PALETTE_BARS,
                   estimator="median", errorbar=None, width=BAR_WIDTH * 2/3, edgecolor=BAR_EDGE)
grid.set_axis_labels("", "")
for ax in grid.axes.flatten():
    ax.set_title("")
for ax in grid.axes[0]:
    col_idx = list(grid.axes[0]).index(ax)
    ax.set_title(grid.col_names[col_idx], fontsize=8)
for i, row_val in enumerate(grid.row_names):
    grid.axes[i, -1].annotate(row_val, xy=(1.05, 0.5), xycoords="axes fraction",
                               fontsize=8, ha="left", va="center", rotation=270)

fig = grid.figure
axs = grid.axes.flatten()

for a in axs:
    a.axhline(1, color=REF_LINE_COLOR)
    a.set_ylim(0.9, 1.1)

bars, _ = apply_hatches(axs, grid.legend)

for bar in bars:
    if bar.get_height() >= 1.1:
        bar.axes.text(
            bar.get_x() + 0.5 * bar.get_width(), 1.105,
            f"{bar.get_height():.1f}", fontsize=5,
            ha="center", va="bottom", rotation=90, color="black",
            zorder=10, clip_on=False)

leg = grid.legend
leg.remove()
fig.legend(loc="lower center", title=None, fontsize=8,
           bbox_to_anchor=(0.5, 1), ncol=1, frameon=False)

fig.supylabel("Runtime relative to Arancini")
fig.set_size_inches((5.91, 3.5))
fig.tight_layout()
savefig(plt, "opt-full")
plt.close("all")

# ── Plot 4: dist ─────────────────────────────────────────────────────────────

print("Generating dist...")

modules = ["Dynamic", "Runtime", "Static", "n. Libraries"]

sns.set_style("ticks", {"xtick.major.size": 8, "ytick.major.size": 8, "axes.grid": False})
sns.set_context("paper", rc={"font.size": 8, "axes.titlesize": 8, "axes.labelsize": 8})

df = pd.read_csv("distribution.csv")
fig, ax = plt.subplots(figsize=(4.72/2, 2.0))

df["short"] = df["short"].apply(to_short)
pf = (df.groupby("short")["what"]
      .value_counts(normalize=True).mul(100)
      .rename('percent').reset_index()
      .sort_values(by=["short"], key=to_idx_dist))
pf["what"] = pf["what"].apply(lambda l: "n. Libraries" if l == "nLib" else l)

ax = sns.histplot(data=pf, y="short", weights="percent", hue="what", multiple="stack",
                  shrink=BAR_WIDTH, edgecolor=BAR_EDGE, palette=PALETTE_BARS,
                  hue_order=modules, ax=ax, line_kws={"linewidth": 4})

ax.spines['bottom'].set_visible(False)
ax.spines['right'].set_visible(False)
ax.set_xlabel("")
ax.set_ylabel("")
ax.set_xticks([])

if BW:
    # Map palette colors to modules (hue_order), assign hatches by module index
    palette_colors = sns.color_palette(PALETTE_BARS, n_colors=len(modules))
    color_to_hatch = {}
    for idx, c in enumerate(palette_colors):
        key = tuple(round(x, 2) for x in c)
        color_to_hatch[key] = hatches[idx]
    hmap_dist = {}
    for bar in ax.patches:
        fc = bar.get_facecolor()
        key = tuple(round(x, 2) for x in fc[:3])  # RGB only, ignore alpha
        if key in color_to_hatch:
            bar.set_hatch(color_to_hatch[key])
            hmap_dist[tuple(round(x, 4) for x in fc)] = color_to_hatch[key]
    for bar in ax.patches:
        bar.set_facecolor("white")
        bar.set_edgecolor("black")

# Emit LaTeX table with distribution numbers
pivot = pf.pivot(index="short", columns="what", values="percent").reindex(
    columns=modules).fillna(0)
# sort by bm_dist order
pivot = pivot.loc[[k for k, v in sorted(bm_dist.items(), key=lambda x: x[1])]]
tex = "\\begin{tabular}{l" + "r" * len(modules) + "}\n"
tex += "\\toprule\n"
tex += " & ".join([""] + modules) + " \\\\\n"
tex += "\\midrule\n"
for idx, row in pivot.iterrows():
    vals = " & ".join(f"{v:.2f}" for v in row)
    tex += f"{idx} & {vals} \\\\\n"
tex += "\\bottomrule\n"
tex += "\\end{tabular}\n"
with open(f"dist-table{SUFFIX}.tex", "w") as f:
    f.write(tex)
print(f"  Saved dist-table{SUFFIX}.tex")

leg = ax.get_legend()
old_handles = leg.get_patches()
old_labels = [t.get_text() for t in leg.get_texts()]
if BW:
    for h in old_handles:
        fc = h.get_facecolor()
        key = tuple(round(x, 2) for x in fc[:3])
        if key in color_to_hatch:
            h.set_hatch(color_to_hatch[key])
        h.set_facecolor("white")
        h.set_edgecolor("black")
leg.remove()

fig.supxlabel("Time spent [%]", y=0.12)
ax.spines['top'].set_visible(False)
fig.legend(old_handles, old_labels, loc="upper center", title=None, fontsize=8,
           bbox_to_anchor=(0.5, 1.13), ncol=2, frameon=False)

fig.add_axes(ax)
fig.tight_layout()
fig.set_size_inches((5.91/2, 2.0))
plt.tight_layout()
savefig(plt, "dist")
plt.close("all")

# ── Plot 5: size ─────────────────────────────────────────────────────────────

print("Generating size...")

def normalize_size(s):
    tmp = s
    base = s[s["Type"] == "x86_64"]
    tmp["Increase"] = tmp["Size [Byte]"].map(lambda l: l / base["Size [Byte]"].median())
    return tmp

sns.set_style("ticks", {"xtick.major.size": 8, "ytick.major.size": 8, "axes.grid": True})
sns.set_context("paper", rc={"font.size": 8, "axes.titlesize": 8, "axes.labelsize": 8})

sf = pd.DataFrame([
    ("libc.so", "x86_64", 763272), ("libc.so", "Tx-Arm", 8729968), ("libc.so", "Tx-RISC-V", 8523632), ("libc.so", "Arm", 881272), ("libc.so", "RISC-V", 2648752),
    ("hi", "x86_64", 10136), ("hi", "Tx-Arm", 205096), ("hi", "Tx-RISC-V", 50160), ("hi", "Arm", 10880), ("hi", "RISC-V", 10368),
    ("km", "x86_64", 13408), ("km", "Tx-Arm", 205480), ("km", "Tx-RISC-V", 75264), ("km", "Arm", 14008), ("km", "RISC-V", 12752),
    ("lr", "x86_64", 10968), ("lr", "Tx-Arm", 204944), ("lr", "Tx-RISC-V", 66288), ("lr", "Arm", 11376), ("lr", "RISC-V", 10760),
    ("mm", "x86_64", 15376), ("mm", "Tx-Arm", 205928), ("mm", "Tx-RISC-V", 92096), ("mm", "Arm", 15864), ("mm", "RISC-V", 14896),
    ("pc", "x86_64", 12536), ("pc", "Tx-Arm", 205656), ("pc", "Tx-RISC-V", 67368), ("pc", "Arm", 13368), ("pc", "RISC-V", 12168),
    ("sm", "x86_64", 13584), ("sm", "Tx-Arm", 205944), ("sm", "Tx-RISC-V", 71680), ("sm", "Arm", 14200), ("sm", "RISC-V", 13160),
    ("wc", "x86_64", 16000), ("wc", "Tx-Arm", 206240), ("wc", "Tx-RISC-V", 88536), ("wc", "Arm", 16584), ("wc", "RISC-V", 15000)
], columns=["Object", "Type", "Size [Byte]"])
sf["Content"] = "whole file"
sf = sf.groupby(["Object"], as_index=False, sort=False)[sf.columns].apply(normalize_size)

sf2 = pd.DataFrame([
    ("libc.so", "x86_64", 367638), ("libc.so", "Tx-Arm", 7041648), ("libc.so", "Tx-RISC-V", 6715382), ("libc.so", "Arm", 395248), ("libc.so", "RISC-V", 289520),
    ("hi", "x86_64", 1604), ("hi", "Tx-Arm", 17772), ("hi", "Tx-RISC-V", 18388), ("hi", "Arm", 1652), ("hi", "RISC-V", 1512),
    ("km", "x86_64", 3608), ("km", "Tx-Arm", 35692), ("km", "Tx-RISC-V", 35254), ("km", "Arm", 3584), ("km", "RISC-V", 2076),
    ("lr", "x86_64", 2112), ("lr", "Tx-Arm", 29652), ("lr", "Tx-RISC-V", 34784), ("lr", "Arm", 1864), ("lr", "RISC-V", 1588),
    ("mm", "x86_64", 3800), ("mm", "Tx-Arm", 48708), ("mm", "Tx-RISC-V", 52446), ("mm", "Arm", 3648), ("mm", "RISC-V", 3104),
    ("pc", "x86_64", 2868), ("pc", "Tx-Arm", 29484), ("pc", "Tx-RISC-V", 29802), ("pc", "Arm", 3016), ("pc", "RISC-V", 2524),
    ("sm", "x86_64", 3088), ("sm", "Tx-Arm", 32540), ("sm", "Tx-RISC-V", 35030), ("sm", "Arm", 2972), ("sm", "RISC-V", 2456),
    ("wc", "x86_64", 4840), ("wc", "Tx-Arm", 48952), ("wc", "Tx-RISC-V", 49426), ("wc", "Arm", 4652), ("wc", "RISC-V", 3872)
], columns=["Object", "Type", "Size [Byte]"])
sf2["Content"] = ".text"
sf2 = sf2.groupby(["Object"], as_index=False, sort=False)[sf2.columns].apply(normalize_size)

size_data = pd.concat([sf, sf2], ignore_index=True)
size_data.drop(size_data[size_data["Type"] == "x86_64"].index, inplace=True)

grid = sns.catplot(data=size_data, kind="bar", height=1.5, width=BAR_WIDTH, aspect=3.15,
                   row="Content", x="Object", hue="Type", palette=PALETTE_BARS,
                   edgecolor=BAR_EDGE, y="Increase")
grid.set_axis_labels("", "")
grid.set_titles(col_template="", row_template="")
for i, row_val in enumerate(grid.row_names):
    grid.axes[i, -1].annotate(row_val, xy=(1.05, 0.5), xycoords="axes fraction",
                               fontsize=8, ha="left", va="center", rotation=270)
grid.set(yscale="log")
grid.tick_params(axis='y', which='minor', left=True)

fig = grid.figure
axs = grid.axes.flatten()

if BW:
    apply_hatches(axs, grid.legend)

leg = grid.legend
leg.remove()
grid.add_legend(loc="lower center", title=None, fontsize=8,
                bbox_to_anchor=(0.5, 0.975), ncol=4, frameon=False)

fig.supylabel("Increase in size")
fig.set_size_inches((5.91, 3.5))
fig.tight_layout()
savefig(plt, "size")
plt.close("all")

print("Done.")
