#! /bin/python3

import csv
import sys
import subprocess as sp
from datetime import datetime, timedelta
import os
import argparse
import configparser
import itertools as it

def main():
    parser = argparse.ArgumentParser(description='Run benchmarks')
    parser.add_argument('name', metavar='n', action="store", nargs='?',
                        help='special name for the symlink', default="latest")
    args = parser.parse_args()
    linkname = args.name
    csvfile = setup(linkname)

    config = configparser.ConfigParser()
    config.read("scripts/conf.ini")
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
    fieldnames = [ "benchmark", "emulator", "time", "type", "threads" ]
    writer = csv.DictWriter(csvfile, fieldnames)
    writer.writeheader()
    return csvfile


def build():
    pass

def do_run(progs, env):
    print(f"Running {progs} ...")
    start = datetime.now()
    env["ARANCINI_ENABLE_LOG"] = "false";
    try:
        out = sp.run(progs, capture_output=True, env=env, timeout=1800)
    except:
        return -1
    end = datetime.now()
    dif = end - start
    if out.returncode != 0:
        print(out.stderr)
        return -1
    return dif/timedelta(microseconds=1)

def to_datafile(pre, name, ver):
    return f"{pre}/{name}_datafiles/{ver}"

def run(csvfile, config): 
    fieldnames = [ "benchmark", "emulator", "time", "type", "threads"]
    writer = csv.DictWriter(csvfile, fieldnames)

    PHOENIX_DIR_PATH = config['x86']['PHOENIX_X86_PATH']
    ARANCINI_RESULT_PATH = config['arancini']['ARANCINI_RESULT_PATH'];
    
    # literaly too lazy to type
    p = PHOENIX_DIR_PATH
    progs = {
        "histogram":to_datafile(p, "histogram", "small.bmp"),
        "kmeans":"",
        "pca":"",
        "string_match":to_datafile(p, "string_match", "key_file_50MB.txt"),
        "matrix_multiply":"1024 1024",
        "word_count":to_datafile(p, "word_count", "word_10MB.txt"),
        "linear_regression":to_datafile(p, "linear_regression", "key_file_50MB.txt")
    }

    native = list(map(lambda l: (l,[ f'{config["native"]["PHOENIX_DIR_PATH"]}{l}', progs[l] ]), progs.keys()))

    arancini = list(map(lambda l: (l,[ f'{config["translations"]["ARANCINI_OUT_PATH"]}{l}.out', progs[l] ]), progs.keys()))
    arancini_dyn = list(map(lambda l: (l,[ f'{config["translations"]["ARANCINI_OUT_PATH"]}{l}-dyn.out', progs[l] ]), progs.keys()))
    arancini_nmem = list(map(lambda l: (l,[ f'{config["translations"]["ARANCINI_OUT_PATH"]}{l}-nmem.out', progs[l] ]), progs.keys()))
    arancini_nlock = list(map(lambda l: (l,[ f'{config["translations"]["ARANCINI_OUT_PATH"]}{l}-nlock.out', progs[l] ]), progs.keys()))

    risotto = list(map(lambda l: (l,[ 'risotto', f'{PHOENIX_DIR_PATH}{l}', progs[l] ]), progs.keys()))
    risotto_qemu = list(map(lambda l: (l,[ 'risotto-qemu', f'{PHOENIX_DIR_PATH}{l}', progs[l] ]), progs.keys()))
    risotto_nofence = list(map(lambda l: (l,[ 'risotto-nofence', f'{PHOENIX_DIR_PATH}{l}', progs[l] ]), progs.keys()))
    risotto_nmem = list(map(lambda l: (l,[ 'risotto', '-nlib', 'general.mni', f'{PHOENIX_DIR_PATH}{l}', progs[l] ]), progs.keys()))
    risotto_nlock = list(map(lambda l: (l,[ 'risotto', '-nlib', 'lock.mni', f'{PHOENIX_DIR_PATH}{l}', progs[l] ]), progs.keys()))

    threads = [ 2, 4, 8 ]

    runs = {
            "native":native,
            "Arancini":arancini,
            "Arancini-nmem":arancini_nmem,
            "Risotto":risotto,
            "Risotto-QEMU":risotto_qemu,
            "Risotto-nofence":risotto_nofence,
            "Risotto-nmem":risotto_nmem,
        }

    # because matrix_multiply wants to be special
    prog = [config["native"]["PHOENIX_DIR_PATH"]+"matrix_multiply"]
    sp.run(prog + ["1024 1024 1"], capture_output=True, timeout=1800) 

    for t in threads:
        for r in runs.keys():
            for b, run in runs[r]:
                for _ in range(5):
                    env = os.environ
                    env["LD_LIBRARY_PATH"] = ARANCINI_RESULT_PATH+"lib"
                    print(f'### {run}')
                    prog = ["taskset", "-c", f"1-{t}"] + run
                    dif = do_run(prog, env)
                    writer.writerow({"benchmark":b, "emulator":r, "time":str(dif), "type":"map-reduce", "threads":f"{t}"})
                    csvfile.flush()

def clean(csvfile):
    pass

if __name__ == "__main__":
    main()
