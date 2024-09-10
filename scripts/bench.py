#! /run/current-system/sw/bin/python3

import csv
import sys
import subprocess as sp
from datetime import datetime, timedelta
import os
import argparse
import configparser
import itertools as it
import json

arch = ""
url = ""
arancini_root = ""
where = os.path.dirname(os.path.realpath(__file__))
config = None

def main():
    global arch
    global url
    global arancini_root
    global where
    global config

    parser = argparse.ArgumentParser(description='Run benchmarks')
    parser.add_argument('name', metavar='n', action="store", nargs='?',
                        help='special name for the symlink', default="latest")
    args = parser.parse_args()
    linkname = args.name
    csvfile = setup(linkname)

    config = json.load(open(f'{where}/conf.json'))

    if (os.uname().machine.startswith("riscv")):
        arch = "riscv64"
    elif (os.uname().machine.startswith("aarch")):
        arch = "aarch64"
    else:
        arch = "x86_64"

    for m in config["machines"]:
        if m["arch"] == arch:
            who = m["url"]
            arancini_root = m["arancini_root"]
            break

    build()
    run(csvfile, config)
    clean(csvfile)

def setup(linkname):
    print("Setup ...")
    print(linkname)
    if not os.path.exists("./bench"):
        os.mkdir("bench")

    time_str = datetime.now().strftime("%Y-%m-%d_%H:%M:%S")
    os.mkdir(f"bench/{time_str}")
    if os.path.islink(f"bench/{linkname}"):
        os.remove(f"bench/{linkname}")
    os.symlink(f"{time_str}", f"bench/{linkname}")

    with open(f"bench/{time_str}/commit", "w+") as c:
        r = sp.run(["git", "rev-parse", "HEAD"], capture_output=True, check=True)
        c.write(r.stdout.decode("utf-8"))

    csvfile = open(f"bench/{time_str}/times.csv", "w+")
    fieldnames = [ "benchmark", "emulator", "time", "threads" ]
    writer = csv.DictWriter(csvfile, fieldnames)
    writer.writeheader()
    return csvfile

def parse_bench(b, obj):
    global arch

    tmp = {}
    tmp["name"] = f'{b["name"]}'
    tmp["guest"] = obj["x86_64"]
    tmp["native"] = obj[arch]
    tmp["args"] = []
    for a in b["args"]:
        if a["type"] == "datafile":
            tmp["args"].append(f'{obj["x86_64"]}/{b["name"]}_datafiles/{a["name"]}')
        else:
            tmp["args"].append(a["name"])
    return tmp

def build():
    pass

def do_run(bench, e, env, t):
    global config

    print(f'Running {bench["name"]} ...')
    start = datetime.now()
    env["ARANCINI_ENABLE_LOG"] = "false";

    cmd = ['taskset', '-c', f'1-{t}']
    if e["what"] == "static":
        tx = e["tx"]
        cmd += ["./"+config["prefixes"][tx]+arch+"/"+bench["name"]+"."+arch]
    elif e["what"] == "dynamic":
        cmd += [e["bin"], f'{bench["guest"]}/{bench["name"]}']
    else:
        cmd += [f'{bench["native"]}/{bench["name"]}']

    print(f'Running: {cmd+bench["args"]}')
    try:
        out = sp.run(cmd+bench["args"], capture_output=True, env=env, timeout=1800)
    except:
        exit -1
    end = datetime.now()
    dif = end - start
    if out.returncode != 0:
        print(out.stderr)
        return -1
    return dif/timedelta(microseconds=1)

def run(csvfile, config): 
    global arch
    global where

    fieldnames = [ "benchmark", "emulator", "time", "threads"]
    writer = csv.DictWriter(csvfile, fieldnames)

    threads = [ 2, 4, 8 ]

    #TODO: make a real path option for the benchmarks
    prog = "./"+config["prefixes"]["translations"]+arch+"/matrix_multiply."+arch
    sp.run([prog]+["1024 1024 1"], capture_output=True, timeout=1800) 

    benchs = config["benchmarks"]["phoenix"]["bins"]

    for b in benchs:
        be = parse_bench(b, config["benchmarks"]["phoenix"])
        for e in config["emulators"]:
            for t in threads:
                for _ in range(5):
                    env = os.environ
                    env["LD_LIBRARY_PATH"] = f'./{config["prefixes"]["results"]}{arch}/lib:./{config["prefixes"]["translations"]}{arch}/'
                    print(env["LD_LIBRARY_PATH"])
                    dif = do_run(be, e, env, t)
                    writer.writerow({"benchmark":be["name"], "emulator":e["name"], "time":str(dif), "threads":f"{t}"})
                    csvfile.flush()

def clean(csvfile):
    pass

if __name__ == "__main__":
    main()
