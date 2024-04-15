#! /bin/python3

import csv
import sys
import subprocess as sp
from datetime import datetime, timedelta
import os
import argparse

def main():
    parser = argparse.ArgumentParser(description='Run benchmarks')
    parser.add_argument('name', metavar='n', action="store",
                        help='special name for the symlink', default="latest")
    args = parser.parse_args()
    linkname = args.name
    csvfile = setup(linkname)
    build()
    run(csvfile)
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
    fieldnames = [ "benchmark", "emulator", "time" ]
    writer = csv.DictWriter(csvfile, fieldnames)
    writer.writeheader()
    return csvfile


def build():
    pass

def do_run(progs, argf, name, env):
    print(f"Running {progs} ...")
    args = ""
    if argf != "":
        args = "../phoenix/phoenix-2.0/"+name+"_datafiles/"+argf
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
def run(csvfile): 
    progs = {"histogram":"small.bmp", "kmeans":"", "pca":"", "string_match":"key_file_50MB.txt", "matrix_multiply":""}
    fieldnames = [ "benchmark", "emulator", "time", "type", "threads"]
    writer = csv.DictWriter(csvfile, fieldnames)
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
                    prog = ["taskset", "-c", f"1-{threads}", "../phoenix/phoenix-2.0/tests/"+p+"/"+p+suffix]
                    dif = do_run(prog, f, p, env)
                    writer.writerow({"benchmark":p, "emulator":"native", "time":str(dif), "type":ty[suffix], "threads":f"{threads}"})

                    #txlat
                    env["LD_LIBRARY_PATH"] = "./"
                    prog = ["taskset", "-c", f"1-{threads}", "../phoenix-arancini/"+p+suffix+"-riscv.out"]
                    dif = do_run(prog, f, p, env)
                    writer.writerow({"benchmark":p, "emulator":"Arancini", "time":str(dif), "type":ty[suffix], "threads":f"{threads}"})
                    csvfile.flush()

                    #txlat-dyn
                    if p not in [ ]:
                        env["LD_LIBRARY_PATH"] = "./"
                        prog = ["taskset", "-c", f"1-{threads}", "../phoenix-arancini/"+p+suffix+"-riscv-dyn.out"]
                        dif = do_run(prog, f, p, env)
                        writer.writerow({"benchmark":p, "emulator":"Arancini-Dyn", "time":str(dif), "type":ty[suffix], "threads":f"{threads}"})
                        csvfile.flush()

                    #QEMU
                    env["LD_LIBRARY_PATH"] = "../phoenix-clang-x86"
                    prog = ["taskset", "-c", f"1-{threads}", "../nixpkgs/result/bin/qemu-x86_64", "../phoenix-clang-x86/"+p+suffix] 
                    dif = do_run(prog, f, p, env)
                    writer.writerow({"benchmark":p, "emulator":"QEMU", "time":str(dif), "type":ty[suffix], "threads":f"{threads}"})
                    csvfile.flush()
                    
                    #MCtoll
                    prog = ["taskset", "-c", f"1-{threads}", "../phoenix-mctoll/"+p+suffix+".ll.out"] 
                    dif = do_run(prog, f, p, env)
                    writer.writerow({"benchmark":p, "emulator":"mctoll", "time":str(dif), "type":ty[suffix], "threads":f"{threads}"})
                    csvfile.flush()

def clean(csvfile):
    print("Cleaning ...")
    close(csvfile)

if __name__ == "__main__":
    main()
