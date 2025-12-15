import pandas as pd
import csv
import matplotlib as mpl
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

bm = { "native":99, "arancini":1, "arancini+nlib":2, "lasagne":98, "arancini-no-fm":3, "arancini-no-fm+nlib":4, "risotto":4, "risotto+nlib":5, "arancini-no-df":3, "arancini-no-df+nlib":4}
sm = { "hi":"hi", "km":"km", "li":"lr", "ma":"mm", "pc":"pc", "st":"sm", "wo":"wc"}

def to_idx(s):
    if s.name != "emulator":
        return s
    return s.apply(lambda b: bm[b])

def gmean(x):
    ret = np.exp(np.mean(np.log(x)))
    return ret

def to_var(s, ver):
    if "-pthread" in s:
        return f'pthread'
    return f'map-reduce'

def to_long_var(s, ver):
    if "-pthread" in s:
        return f'{ver}: pthread'
    return f'{ver}: map-reduce'

def to_short(s):
    return sm[s]

opt = []
scale = []

append = []
csvfile = open("../bench/latest/times.csv", "r")
append.append(pd.read_csv(csvfile))
## add any other arm run here
base = pd.concat(append, ignore_index=True)

base["version"] = "Arm"

def normalize(s, e, c):
    tmp = s
    base = s[s[c]==e]
    tmp[f'normalized_time_{e}'] = tmp["time"].map(lambda l: l/base["time"].median())
    return tmp

def inv_normalize(s, e, c):
    tmp = s
    base = s[s[c]==e]
    tmp[f'normalized_time_{e}'] = tmp[["time", "threads"]].apply(lambda l: base["time"].median()/(l["time"]*l["threads"]), axis=1)
    return tmp


base.drop(base[base["time"]==-1.0].index, inplace=True)

opt.append(base.copy())
scale.append(base.copy())

# RISC-V data
# append = []
# csvfile = open("../bench/risc-v/times.csv", "r")
# append.append(pd.read_csv(csvfile))
#
# base = pd.concat(append, ignore_index=True)
# base["version"] = "RISC-V"
# base.drop(base[base["time"]==-1.0].index, inplace=True)
#
# opt.append(base.copy())
# scale.append(base.copy())

# optimizations
for i in [0]:
    opt[i].drop(opt[i][opt[i]["emulator"]=="risotto"].index, inplace=True)
    opt[i].drop(opt[i][opt[i]["emulator"]=="risotto-nofence"].index, inplace=True)
    opt[i].drop(opt[i][opt[i]["emulator"]=="risotto+nlib"].index, inplace=True)
    opt[i].drop(opt[i][opt[i]["emulator"]=="lasagne"].index, inplace=True)
    opt[i].drop(opt[i][opt[i]["emulator"]=="mctoll"].index, inplace=True)
    opt[i]["short"] = opt[i]["benchmark"].apply(lambda l: to_short(l[0:2]))
    arch = "Arm"
    if i == 1:
        arch = "RISC-V"
    opt[i]["variant"] = opt[i]["benchmark"].apply(to_var, ver=arch)
    opt[i].sort_values(by=["emulator", "version", "benchmark"], key=to_idx, inplace=True)

# big plots
for i in [0]:
    scale[i].drop(scale[i][scale[i]["emulator"]=="arancini-no-df"].index, inplace=True)
    scale[i].drop(scale[i][scale[i]["emulator"]=="arancini-no-df+nlib"].index, inplace=True)
    scale[i].drop(scale[i][scale[i]["emulator"]=="arancini-no-fm"].index, inplace=True)
    scale[i].drop(scale[i][scale[i]["emulator"]=="arancini-no-fm+nlib"].index, inplace=True)


    scale[i] = scale[i].groupby(["benchmark", "threads"], as_index=False)[scale[i].columns].apply(normalize, "native", "emulator")
    scale[i] = scale[i].groupby(["benchmark", "emulator"], as_index=False)[scale[i].columns].apply(inv_normalize, 1, "threads")
    scale[i]["short"] = scale[i]["benchmark"].apply(lambda l: to_short(l[0:2]))
    arch = "Arm"
    if i == 1:
        arch = "RISC-V"
    scale[i]["variant"] = scale[i]["benchmark"].apply(to_var, ver=arch)
    scale[i].sort_values(by=["emulator", "benchmark"], key=to_idx, inplace=True)

    #scale[i].drop(scale[i][scale[i]["emulator"]=="native"].index, inplace=True)

data = pd.concat(scale, ignore_index=True)
data2 = data.copy()
data.drop(data[data["emulator"]=="native"].index, inplace=True)

mpl.rcParams["pdf.fonttype"] = 42
mpl.rcParams["ps.fonttype"] = 42
mpl.rcParams["figure.labelsize"] = 14
#mpl.rcParams["figure.figsize"] = (7,3)
#fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(7, 3), sharex=True)
#fig.subplots_adjust(hspace=0.05)

#sns.set_style("whitegrid")
sns.set_style("ticks", {"xtick.major.size": 8, "ytick.major.size": 8})
sns.set_context("paper", rc={"font.size": 5, "axes.titlesize": 5, "axes.labelsize": 8})

sns.set_style("ticks", {"xtick.major.size": 14, "ytick.major.size": 14, "axes.grid":True})
sns.set_context("talk", rc={"font.size": 14, "axes.titlesize": 18, "axes.labelsize": 14})

dat = data.copy()
#dat.drop(dat[dat["emulator"]=="arancini"].index, inplace=True)
dat.drop(dat[dat["emulator"]=="risotto"].index, inplace=True)

dat["emulator"] = dat["emulator"].apply(lambda l: "Arancini" if l=="arancini+nlib" else l)
dat["emulator"] = dat["emulator"].apply(lambda l: "Risotto" if l=="risotto+nlib" else l)
dat["emulator"] = dat["emulator"].apply(lambda l: "Lasagne" if l=="lasagne" else l)
grid = sns.catplot(data=dat[dat["threads"]==16],row="version", col="variant", kind="bar", height=2,
                   aspect=1, x="short", y="normalized_time_native", hue="emulator",
                   palette="pastel", estimator=gmean, width=0.9, edgecolor="black")
#sns.barplot(data=scale[0][scale[0]["threads"] == 16], x="short", y="normalized_time", hue="emulator", palette="pastel", ax=ax1, estimator="median", width=0.5, edgecolor="black")
#sns.barplot(data=scale[1][scale[1]["threads"] == 16], x="short", y="normalized_time", hue="emulator", palette="pastel", ax=ax2, estimator="median", width=0.5, edgecolor="black", legend=False)

grid.set_axis_labels("", "")
grid.set_titles("{row_name}: {col_name}")

fig = grid.figure
fig.supylabel("Runtime relative to native")
axs = grid.axes.flatten()

for a in axs:
    a.set_ylim(0, 20)

#ax1.spines.bottom.set_visible(False)
#ax2.spines.top.set_visible(False)
#ax1.tick_params(axis='x', which='both', bottom=False, top=False, labelbottom=False)
#ax2.xaxis.tick_bottom()

#ax2.set_xticklabels(scale[0]["short"].unique(), rotation=0)
    # ax.legend(loc="upper left", title=None, fontsize=FONTSIZE,
    #           bbox_to_anchor=(-0.02, 1.0))
    # place legend below the graph

#ax2.set_ylabel("Runtime Relative to Native")
#ax2.yaxis.set_label_coords(-0.05, 1.2) 
#ax2.set_xlabel("")
#ax1.set_ylabel("")
#ax1.set_xlabel("")


#d = .5  # proportion of vertical to horizontal extent of the slanted line
#kwargs = dict(marker=[(-1, -d), (1, d)], markersize=12,
#              linestyle="none", color='k', mec='k', mew=1, clip_on=False)
#ax1.plot([0, 1], [0, 0], transform=ax1.transAxes, **kwargs)
#ax2.plot([0, 1], [1, 1], transform=ax2.transAxes, **kwargs)

#hatches = ["/", "-", "o", "\\", "---", ".", "x"]
hatches = ["..", "--", "//", "\\\\", "xx", "OO"]

bars = []
for ax in axs:
    for p in ax.patches:
        bars.append(p)

handles = grid.legend.get_patches()
#bars = ax1.patches
#handles = ax1.get_legend().get_patches()
l = len(handles)

hs = []
hmap = {}

i=0
for bar in bars:
    if bar.get_facecolor() not in hmap.keys():
        hmap[bar.get_facecolor()] = hatches[i]
        i=i+1
    bar.set_hatch(hmap[bar.get_facecolor()])

    if bar.get_height() >= 22:
        bar.axes.text(bar.get_x()+bar.get_width()/2.0, 20-0*list(hmap.keys()).index(bar.get_facecolor()),
                      str(int(bar.get_height())), backgroundcolor=None, fontsize=7,
                      bbox={"boxstyle":"round", "pad":0.1, "fc":"w"},
                      ha="center", rotation="horizontal")



#for b in ax2.patches:
#    b.set_hatch(hmap[b.get_facecolor()])

for h in handles:
    h.set_hatch(hmap[h.get_facecolor()])

#leg = ax1.get_legend()
leg = grid.legend
old_handles = leg.get_patches()
old_labels = list(map(lambda t: t.get_text(), leg.get_texts()))

leg.remove()

grid.add_legend(
    #old_handles,
    #old_labels,
    loc="lower center",
    title=None,
    fontsize=14,
    bbox_to_anchor=(0.5, 1),
    ncol=5,
    frameon=False,
)

#ax2.axhline(y=1, color="black", linewidth=1)

fig.set_size_inches((8, 5))
fig.tight_layout()

plt.savefig("figure6.png", dpi=500, bbox_inches="tight")

out = ["risotto", "risotto+nlib", "lasagne", "arancini+nlib"]
#ob = ["km", "lr", "sm", "pc"]
#data2=data.copy()

sns.set_style("ticks", {"xtick.major.size": 18, "ytick.major.size": 18, "axes.grid":True})
sns.set_context("talk", rc={"font.size": 18, "axes.titlesize": 18, "axes.labelsize": 18})

for o in out:
    data2.drop(data2[data2["emulator"]==o].index, inplace=True)

#for o in ob:
#    data2.drop(data2[data2["short"]==o].index, inplace=True)
#tf = tf.groupby(["benchmark", "emulator"], as_index=False)[tf.columns].apply(inv_normalize, 1, "threads")
#tf["short"] = tf["benchmark"].apply(lambda l: l[0:2])
#tf.sort_values(by=["emulator", "benchmark"], key=to_idx, inplace=True)

#fig, ax = plt.subplots(1, figsize=(7, 3), sharex=True)

#grid = sns.catplot(data=tf, x="threads", height=3, aspect=0.16, y="normalized_time", kind="bar", col="short", hue="emulator",edgecolor="black", palette="pastel", estimator="median")
grid = sns.catplot(data=data2,row="version", col="short", kind="point",
                   linestyles=["solid", "dashed", "dotted"], height=4, aspect=2,
                   x="threads", y="normalized_time_1", hue="emulator", palette="pastel",
                   estimator=gmean, markers=None, errorbar=None, linewidth = 3) #, edgecolor="black")

grid.set_axis_labels("", "")
grid.set_titles("{row_name} : {col_name}")

#grid.set(xticks=["1","2","8","64"])
grid.set(yticks=[0.25, 0.5, 0.75, 1])

fig = grid.figure

fig.supxlabel("#Threads")

axs = grid.axes.flatten()

bars = []
for ax in axs:
    for p in ax.patches:
        bars.append(p)

handles = grid.legend.get_patches()
l = len(handles)

hs = []
hmap = {}

i=0
for bar in bars:
    if bar.get_facecolor() not in hmap.keys():
        hmap[bar.get_facecolor()] = hatches[i]
        i=i+1
    bar.set_hatch(hmap[bar.get_facecolor()])


#for b in bars:
#    b.set_hatch(hmap[b.get_facecolor()])

for h in handles:
    h.set_hatch(hmap[h.get_facecolor()])

leg = grid.legend
old_handles = handles
old_labels = list(map(lambda t: t.get_text(), leg.get_texts()))

leg.remove()

fig.legend(
    #legend_data=list(zip(old_handles,old_labels)),
    loc="lower center",
    title=None,
    fontsize=14,
    bbox_to_anchor=(0.5, 0.95),
    ncol=3,
    frameon=False,
)

fig.supylabel("Relative scaling factor")
fig.set_size_inches((18, 5))
fig.tight_layout()

grid.savefig("figure9.png", dpi=500)

dota = pd.concat(opt, ignore_index=True)
fm = dota.copy()

fm.drop(fm[fm["emulator"]=="native"].index, inplace=True)
#fm.drop(fm[fm["emulator"]=="arancini-no-df"].index, inplace=True)
fm.drop(fm[fm["emulator"]=="arancini-no-df+nlib"].index, inplace=True)
fm.drop(fm[fm["emulator"]=="arancini-no-fm+nlib"].index, inplace=True)
fm.drop(fm[fm["emulator"]=="arancini+nlib"].index, inplace=True)

tmp = fm[fm["emulator"]=="arancini"]

def to_opt(e):
    if "df" in e:
        return "df"
    if "fm" in e:
        return "fm"
    return ""

def adj_name(e):
    if "df" in e:
        return "arancini w/o opt"
    if "fm" in e:
        return "arancini w/o opt"
    return "arancini w/ opt"

fm["name"] = fm[["emulator", "version"]].apply(lambda l: f'{adj_name(l["emulator"])} - {l["version"]}', axis=1)
fm["opt"] = fm["emulator"].apply(to_opt)

based = fm[fm["opt"]==""]
b1 = based.copy()
b1["opt"] = "df"
b2 = based.copy()
b2["opt"] = "fm"

fm.drop(based.index, inplace=True)
fm = pd.concat([fm, b1, b2], ignore_index=True)

fm = fm.groupby(["benchmark", "threads", "version", "variant"], as_index=False)[fm.columns].apply(normalize, "arancini", "emulator")
fm.sort_values(by=["emulator", "benchmark"], key=to_idx, inplace=True)


fm.drop(fm[fm["emulator"]=="arancini"].index, inplace=True)
#fig, ax = plt.subplots(1, figsize=(3.5, 3), sharex=True)

sns.set_style("ticks", {"xtick.major.size": 18, "ytick.major.size": 18, "axes.grid":True})
sns.set_context("talk", rc={"font.size": 18, "axes.titlesize": 18, "axes.labelsize": 18})

#ax = sns.barplot(ax=ax, data=fm, x="short", y="normalized_time", hue="emulator",edgecolor="black", palette="pastel", estimator="median", width=0.5)

grid = sns.catplot(data=fm, row="opt", col="variant", kind="bar", height=4, aspect=2,
                   x="short", y="normalized_time_arancini", hue="name", palette="pastel",
                   estimator="median", errorbar=None, width=0.75, edgecolor="black")



grid.set_axis_labels("", "")
grid.set_titles("{row_name}: {col_name}")

fig = grid.figure

axs = grid.axes.flatten()

mut = sns.color_palette(palette="dark")

for a in axs:
    a.axhline(1, color=mut[2])
    a.set_ylim(0.9, 1.1)

bars = []
for ax in axs:
    for p in ax.patches:
        bars.append(p)

handles = grid.legend.get_patches()
l = len(handles)

hs = []
hmap = {}

i=0
for bar in bars:
    if bar.get_facecolor() not in hmap.keys():
        hmap[bar.get_facecolor()] = hatches[i]
        i=i+1
    bar.set_hatch(hmap[bar.get_facecolor()])
    if bar.get_height() >= 1.1:
        bar.axes.text(bar.get_x()+0.5*bar.get_width(), 1.1-0*list(hmap.keys()).index(bar.get_facecolor()), str(round(bar.get_height(), 1)), backgroundcolor="w", fontsize=6, bbox={"boxstyle":"round", "pad":0.1, "fc":"w"}, ha="center")


for h in handles:
    h.set_hatch(hmap[h.get_facecolor()])

leg = grid.legend
old_handles = handles
old_labels = list(map(lambda t: t.get_text(), leg.get_texts()))

leg.remove()

fig.legend(
    #legend_data=list(zip(old_handles,old_labels)),
    loc="lower center",
    title=None,
    fontsize=18,
    bbox_to_anchor=(0.5, 1),
    ncol=1,
    frameon=False,
    #bbox_transform=fig.get_transform()
    #mode="expand"
)

fig.supylabel("Runtime relative to Arancini")
fig.set_size_inches((8, 5))
fig.tight_layout()

plt.savefig("figure8.png", dpi=500, bbox_inches="tight")
