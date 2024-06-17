#! /bin/python3

import csv
import sys
import subprocess as sp
from datetime import datetime, timedelta
import os
import argparse
import configparser

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

def do_run(progs, argf, name, env, config):
    print(f"Running {progs} ...")
    args = ""
    if argf != "":
        args = config['x86']['PHOENIX_X86_PATH']+"/"+name+"_datafiles/"+argf
    start = datetime.now()
    env["ARANCINI_ENABLE_LOG"] = "false";
    try:
        out = sp.run(progs + [args], capture_output=True, env=env, timeout=1800)
    except:
        return -1
    end = datetime.now()
    dif = end - start
    if out.returncode != 0:
        print(out.stderr)
        return -1
    return dif/timedelta(microseconds=1)

ty = { "":"map-reduce", "-seq":"sequential", "-pthread":"pthreads" }
def run(csvfile, config): 
    progs = {"histogram":"small.bmp", "kmeans":"", "pca":"", "string_match":"key_file_50MB.txt", "matrix_multiply":""}
    fieldnames = [ "benchmark", "emulator", "time", "type", "threads"]
    writer = csv.DictWriter(csvfile, fieldnames)

    PHOENIX_DIR_PATH = config['x86']['PHOENIX_X86_PATH']
    ARANCINI_RESULT_PATH = config['arancini']['ARANCINI_RESULT_PATH'];
    ARANCINI_OUT_PATH = config['translations']['ARANCINI_OUT_PATH'];
    MCTOLL_OUT_PATH = config['translations']['MCTOLL_OUT_PATH'];
    QEMU_PATH = config['translations']['QEMU_PATH'];

    # because matrix_multiply wants to be special
    prog = [config["native"]["PHOENIX_DIR_PATH"]+"matrix_multiply"]
    sp.run(prog + ["1024 1024 1"], capture_output=True, timeout=1800) 

    for p in progs.keys():
        f = progs[p]
        for suffix in ["", "-seq", "-pthread"]:
            th = [1]
            if suffix != "-seq":
                th = [1, 2, 4, 8];
            
            for threads in th:
                for i in range(3):
                    #native
                    env = os.environ
                    prog = ["taskset", "-c", f"1-{threads}", config["native"]["PHOENIX_DIR_PATH"]+p+suffix]
                    dif = do_run(prog, f, p, env, config)
                    writer.writerow({"benchmark":p, "emulator":"native", "time":str(dif), "type":ty[suffix], "threads":f"{threads}"})

                    #txlat
                    env["LD_LIBRARY_PATH"] = ARANCINI_RESULT_PATH+"lib"
                    prog = ["taskset", "-c", f"1-{threads}", ARANCINI_OUT_PATH+p+suffix+".out"]
                    dif = do_run(prog, f, p, env, config)
                    writer.writerow({"benchmark":p, "emulator":"Arancini", "time":str(dif), "type":ty[suffix], "threads":f"{threads}"})
                    csvfile.flush()

                    #txlat-dyn
                    env["LD_LIBRARY_PATH"] = ARANCINI_RESULT_PATH+"/lib"
                    prog = ["taskset", "-c", f"1-{threads}", ARANCINI_OUT_PATH+p+suffix+"-dyn.out"]
                    dif = do_run(prog, f, p, env, config)
                    writer.writerow({"benchmark":p, "emulator":"Arancini-Dyn", "time":str(dif), "type":ty[suffix], "threads":f"{threads}"})
                    csvfile.flush()

                    #txlat-nofence
                    env["LD_LIBRARY_PATH"] = ARANCINI_RESULT_PATH+"/lib"
                    prog = ["taskset", "-c", f"1-{threads}", ARANCINI_OUT_PATH+p+suffix+"-nofence.out"]
                    dif = do_run(prog, f, p, env, config)
                    writer.writerow({"benchmark":p, "emulator":"Arancini-No-Fence", "time":str(dif), "type":ty[suffix], "threads":f"{threads}"})
                    csvfile.flush()

                    #QEMU
                    env["LD_LIBRARY_PATH"] = PHOENIX_DIR_PATH
                    prog = ["taskset", "-c", f"1-{threads}", QEMU_PATH, PHOENIX_DIR_PATH+p+suffix] 
                    dif = do_run(prog, f, p, env, config)
                    writer.writerow({"benchmark":p, "emulator":"QEMU", "time":str(dif), "type":ty[suffix], "threads":f"{threads}"})
                    csvfile.flush()
                    
                    #MCtoll
                    #prog = ["taskset", "-c", f"1-{threads}", MCTOLL_OUT_PATH+p+suffix+".ll.out"] 
                    #dif = do_run(prog, f, p, env)
                    #writer.writerow({"benchmark":p, "emulator":"mctoll", "time":str(dif), "type":ty[suffix], "threads":f"{threads}"})
                    #csvfile.flush()

def clean(csvfile):
    print("Cleaning ...")
    close(csvfile)

if __name__ == "__main__":
    main()
