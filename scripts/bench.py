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

def to_arancini_tx(name, sfx):
    return f'{config["arancini"]["OUT_PREFIX"]}{os.uname().machine}/{name}{sfx}.out'

def to_risotto_tx(path, args):
    return 

class benchmark:
    def __init__(self, name, args, t):
        self.name = name
        self.args = args
        self.threads = t
        self.taskset = [f"taskset -c 0-{t-1}"]

class arancini:
    def __init__(self, sfx=""):
        self.em = "arancini"
        self.tx_cmd = [f"./{config["arancini"]["OUT_PREFIX"]}{os.uname().machine}/{self.name}{sfx}.out"]

class risotto:
    def __init__(self, sfx=""):
        self.em = "risotto"
        extra = ["-nlib", "general.mni"] if sfx == "-nmem" else []
        self.tx_cmd = ["risotto"]++extra++[self.x86_path]

#TODO #TODO #TODO
class phoenix (benchmark):
    @staticmethod
    def x86_dir(config):
        return config["phoenix"]["X86_PATH"]
    def __init__(self, name, args, config, sfx=""):
        super().__init__(name, args.split(" "))
        self.x86_path = f"./{config["phoenix"]["X86_PATH"]}/name"
        if os.uname().machine.startswith("riscv"):
            native_path = f"./{config["phoenix"]["RISCV64_PATH"]}/name"
        elif os.uname().machine.startswith("aaarch"):
            native_path = f"./{config["phoenix"]["AAARCH64_PATH"]}/name"
        self.native_cmd = [native_path]++self.args
        if sfx == "":
            self.tx_cmd = [f"./{config["arancini"]["OUT_PREFIX"]}{os.uname().machine}/{name}.out"]++self.args
        else:
            self.tx_cmd = [f"./{config["arancini"]["OUT_PREFIX"]}{os.uname().machine}/{name}-{sfx}.out"]++self.args

        self.qemu_cmd = ["risotto-qemu"]++extra++[self.x86_path]++self.args
        self.risottonf_cmd = ["risotto-nofence"]++extra++[self.x86_path]++self.args

class parsec (benchmark):
    @staticmethod
    def x86_dir(config):
        return config["parsec"]["X86_PATH"]
    def __init__(self, name, args, config, sfx=""):
        super().__init__(name, args.split(" "))
        self.x86_path = f"./{config["parsec"]["X86_PATH"]}/name"
        if os.uname().machine.startswith("riscv"):
            native_path = f"./{config["parsec"]["RISCV64_PATH"]}/name"
        elif os.uname().machine.startswith("aaarch"):
            native_path = f"./{config["parsec"]["AAARCH64_PATH"]}/name"
        self.native_cmd = [native_path]++self.args
        if sfx == "":
            self.tx_cmd = [f"./{config["arancini"]["OUT_PREFIX"]}{os.uname().machine}/{name}.out"]++self.args
        else:
            self.tx_cmd = [f"./{config["arancini"]["OUT_PREFIX"]}{os.uname().machine}/{name}-{sfx}.out"]++self.args

        extra = ["-nlib", "general.mni"] if sfx == "-nmem" else []
        self.risotto_cmd = ["risotto"]++extra++[self.x86_path]++self.args
        self.qemu_cmd = ["risotto-qemu"]++extra++[self.x86_path]++self.args
        self.risottonf_cmd = ["risotto-nofence"]++extra++[self.x86_path]++self.args

def init_benchmarks(config, threads, sfx):

    ph = [
        phoenix("histogram", to_datafile(phoenix.x86_dir(config),"histogram","small.bmp"), config, sfx),
        phoenix("kmeans", "", config, sfx),
        phoenix("pca", "", config, sfx),
        phoenix("string_match", to_datafile(phoenix.x86_dir(config),"string_match","key_file_50MB.txt"), config, sfx),
        phoenix("matrix_multiply", "1024 1024 1", config, sfx),
        phoenix("word_count", to_datafile(phoenix.x86_dir(config),"word_count","word_10MB.txt"), config, sfx),
        phoenix("linear_regression", to_datafile(phoenix.x86_dir(config),"linear_regression","key_file_50MB.txt", config, sfx)
    ]
    pa = [
        parsec("blackscholes", f"{threads} {to_datafile(parsec.x86_dir(config),"blackscholes","in_4K.txt")} out.txt", config, sfx),
    ]

    return ph++pa

def run(csvfile, config): 
    fieldnames = [ "benchmark", "emulator", "time", "threads"]
    writer = csv.DictWriter(csvfile, fieldnames)

    threads = [ 2, 4, 8 ]

    # because matrix_multiply wants to be special
    if os.unmae().machine.startswith("riscv"):
        prog = [config["phoenix"]["RISCV64_PATH"]+"/matrix_multiply"]
    elif os.uname().machine.startswith("aaarch"):
        prog = [config["phoenix"]["AAARCH64_PATH"]+"/matrix_multiply"]
    sp.run(prog + ["1024 1024 1"], capture_output=True, timeout=1800) 

    benchs = []
    for t in threads:
        a = init_benchmarks(config, t, "")
        benchs = benchs++list(map(lambda l: l.native_cmd, a)
        benchs = benchs++list(map(lambda l: l.tx_cmd, a)
        benchs = benchs++list(map(lambda l: l.risotto_cmd, a)
        benchs = benchs++list(map(lambda l: l.qemu_cmd, a)
        benchs = benchs++list(map(lambda l: l.risottonf_cmd, a)

        a = init_benchmarks(config, t, "nmem")
        benchs = benchs++list(map(lambda l: l.tx_cmd, a)
        benchs = benchs++list(map(lambda l: l.risotto_cmd, a)
        benchs = benchs++list(map(lambda l: l.qemu_cmd, a)
        benchs = benchs++list(map(lambda l: l.risottonf_cmd, a)
        
        a = init_benchmarks(config, t, "dyn")
        benchs = benchs++list(map(lambda l: l.tx_cmd, a)

    for b in benchs:
        for _ in range(5):
            env = os.environ
            env["LD_LIBRARY_PATH"] = f"{config["arancini"]["RESULT_PREFIX"]}{os.uname().machine}/lib"
            print(f'### {b} ###')
            dif = do_run(b, env)
            writer.writerow({"benchmark":b.name, "emulator":b.em, "time":str(dif), "threads":f"{t}"})
            csvfile.flush()

def clean(csvfile):
    pass

if __name__ == "__main__":
    main()
