import pandas as pd
import csv
import matplotlib as mpl
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle 
import seaborn as sns
columns=["benchmark", "where", "what"]
benchs=["histogram", "kmeans", "linear_regression", "matrix_multiply", "pca", "string_match", "word_count"]
sm = { "hi":"hi", "km":"km", "li":"lr", "ma":"mm", "pc":"pc", "st":"sm", "wo":"wc"}
hatches = ["..", "--", "//", "\\", "xx", "OO"]
modules = ["Dynamic", "Runtime", "Static", "n. Libraries"]

# colors = ["#38761d", "#990000", "#0065bd", "#b45f06"]
# google = sns.set_palette(sns.color_palette(colors))

bm = { "hi":0, "lr":2, "mm":3, "pc":4, "sm":5, "km":1, "wc":6}

def to_idx(s):
    return s.apply(lambda b: bm[b])

def to_short(s):
    return sm[s]

def parse(line, bench):
    if "kernel" in line:
        return pd.DataFrame()
    file = ""
    what = ""
    if line.split(" ")[1] == "[unknown]":
        what = "Dynamic"
        return pd.DataFrame([[bench, "", what]], columns=columns)

    last = line.split(" ")[-1]
    file = last.split("/")[-1].split(")")[0]

    if file in ["libarancini-runtime.so", "libarancini-input-x86.so", "libarancini-ir.so", "libkeystone.so.0", "libstdc++.so.6.0.32", "ld-linux-aarch64.so.1"]:
        what = "Runtime"
    if file == "libc.so.6":
        what = "nLib"
    if file.split(".")[-1] == "aarch64":
        what = "Static"

    if what == "":
        #print(line)
        return pd.DataFrame()

    return pd.DataFrame([[bench, file, what]], columns=columns)

def mk_df():
    df = pd.DataFrame(columns=["benchmark", "where", "what"])
    for b in benchs:
        LOGFILE = f'../{b}-pthread.data.script'

        with open(LOGFILE, "r") as logfile:
            for line in logfile:

                if line[1:3] in b:
                    df = pd.concat([df, parse(line, b)], ignore_index=True)
    df["short"] = df["benchmark"].apply(lambda l: l[0:2])
    return df

def plot():

    df = pd.read_csv("distribution.csv")
    #df = mk_df()

    #mpl.use("Agg")
    #mpl.rcParams["text.latex.preamble"] = r"\usepackage{amsmath}"
    mpl.rcParams["pdf.fonttype"] = 42
    mpl.rcParams["ps.fonttype"] = 42
    mpl.rcParams["figure.labelsize"] = 18
    #mpl.rcParams["font.family"] = "libertine"
    fig, ax = plt.subplots(figsize=(8, 4))

    #sns.set_style("whitegrid")
    #sns.set_style("ticks", {"xtick.major.size": 8, "ytick.major.size": 8})
    #sns.set_context("paper", rc={"font.size": 5, "axes.titlesize": 5, "axes.labelsize": 8})

    sns.set_style("ticks", {"xtick.major.size": 18, "ytick.major.size": 18})
    sns.set_context("talk", rc={"font.size": 18, "axes.titlesize": 18, "axes.labelsize": 18})

    df["short"] = df["short"].apply(to_short)

    pf = (df
    .groupby("short")["what"]
    .value_counts(normalize=True)
    .mul(100)
    .rename('percent')
    .reset_index()
    .sort_values(by=["short"], key=to_idx))
    pf["what"] = pf["what"].apply(lambda l: "n. Libraries" if l=="nLib" else l) 
    #pf["col"] = pf["short"].apply(lambda l: bm[l]<4)
    #pf = pd.concat([pf, pd.DataFrame([("", "Static", 0.1, False)])], ignore_index=True)

    #grid = sns.FacetGrid(pf, col="col", sharey=False)
    ax = sns.histplot(data=pf, y="short", weights="percent", hue="what", multiple="stack",
                      shrink=0.75, edgecolor=None, palette="pastel", hue_order=modules, ax=ax, line_kws={"linewidth":4})

    # not all categories are plotted sometime

    #axs = grid.ax.flatten()
    #axs[1].yaxis.tick_right()
    #axs[1].spines['right'].set_visible(True)
    #axs[1].spines['left'].set_visible(False)
    #axs[1].margins(x=0)

    #grid.set_axis_labels("", "")
    #grid.set_titles("")

    hmap = {}
    ax.spines['bottom'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.set_xlabel("")
    ax.set_ylabel("")
    ax.set_xticks([])
    bars = ax.patches

    handles = ax.get_legend().get_patches()
    l = len(handles)

    hs = []

    i=0
    for bar in bars:
        if bar.get_facecolor() not in hmap.keys():
            hmap[bar.get_facecolor()] = hatches[i]
            i=i+1
        #bar.set_hatch(hmap[bar.get_facecolor()])

        if bar.get_width() > 28:
            rx, ry = bar.get_xy()
            cx = rx + bar.get_width()/2.0
            cy = ry + bar.get_height()/2.0

            ax.annotate(str(round(bar.get_width(), 2)), (cx, cy), color='k', 
                    fontsize=18, ha='center', va='center', bbox={'alpha':0 , 'pad':0, 'boxstyle':"round", 'linewidth':0})


    #for b in ax.patches:
    #    #b.set_hatch(hmap[b.get_facecolor()])
    #    if b.get_width() > 10:
    #        rx, ry = b.get_xy()
    #        cx = rx + b.get_width()/2.0
    #        cy = ry + b.get_height()/2.0

    #        ax.annotate(str(round(b.get_width(), 2)), (cx, cy), color='k', 
    #            fontsize=8, ha='center', va='center', bbox={'alpha': 0, 'pad':0, 'boxstyle':"round", 'linewidth':0})
        #h.set_hatch(hmap[h.get_facecolor()])

    leg = ax.get_legend()
    old_handles = leg.get_patches()
    old_labels = list(map(lambda t: t.get_text(), leg.get_texts()))

    leg.remove()

    #ax.set_xticklabels(be, rotation=90)
        # ax.legend(loc="upper left", title=None, fontsize=FONTSIZE,
        #           bbox_to_anchor=(-0.02, 1.0))
        # place legend below the graph

    fig.supxlabel("Time spent [%]")
    ax.spines['top'].set_visible(False)
    leg = fig.legend(
        old_handles,
        old_labels,
        loc="center left",
        title=None,
        fontsize=18,
        bbox_to_anchor=(0.95, 0.5),
        ncol=1,
        frameon=False,
    )

    fig.add_axes(ax)
    fig.tight_layout()
    fig.set_size_inches((8, 5))
    plt.tight_layout()

    plt.savefig("dist.png", dpi=500, bbox_inches="tight")

plot()
