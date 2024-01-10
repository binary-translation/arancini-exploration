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

def do_run(progs, argf, name):
    print(f"Running {progs} ...")
    args = ""
    if argf != "":
        args = "test/phoenix/"+name+"_datafiles/"+argf
    start = datetime.now()
    out = sp.run(progs + [args], capture_output=True)
    end = datetime.now()
    dif = end - start
    if out.returncode != 0:
        print(out.stderr)
        dif = timedelta(0)
    return dif/timedelta(microseconds=1)

def run(csvfile): 
    progs = {"histogram":"small.bmp", "kmeans":"", "pca":"", "string_match":"key_file_50MB.txt", "matrix_multiply":""}
    fieldnames = [ "benchmark", "emulator", "time" ]
    writer = csv.DictWriter(csvfile, fieldnames)
    for p in progs.keys():
        f = progs[p]
        for i in range(50):
            #native
            prog = ["/share/simonk/static-musl-phoenix/"+p+"-seq"]
            #prog = ["/share/sebastian/phoenix/"+p+"-seq"]
            dif = do_run(prog, f, p)
            writer.writerow({"benchmark":p, "emulator":"native", "time":str(dif)})

            #txlat
            prog = ["./"+p+"-seq-static-musl-riscv.out"]
            #prog = ["./"+p+"-seq-static-musl-arm.out"]
            dif = do_run(prog, f, p)
            writer.writerow({"benchmark":p, "emulator":"Arancini", "time":str(dif)})
            csvfile.flush()

            #txlat-dyn
            if p not in [ ]:
                prog = ["./"+p+"-seq-static-musl-riscv-dyn.out"]
                #prog = ["./"+p+"-seq-static-musl-arm-dyn.out"]
                dif = do_run(prog, f, p)
                writer.writerow({"benchmark":p, "emulator":"Arancini-Dyn", "time":str(dif)})
                csvfile.flush()

            #QEMU
            prog = ["/nix/store/i2k6sywwzf0vka7p09asf66g651kmxa8-qemu-riscv64-unknown-linux-gnu-8.0.0/bin/qemu-x86_64", "/share/simonk/static-musl-phoenix/"+p+"-seq-static-musl"] 
            #prog = ["qemu-x86_64", "/share/simonk/static-musl-phoenix/"+p+"-seq-static-musl"] 
            dif = do_run(prog, f, p)
            writer.writerow({"benchmark":p, "emulator":"QEMU", "time":str(dif)})
            csvfile.flush()

def clean(csvfile):
    print("Cleaning ...")
    close(csvfile)

if __name__ == "__main__":
    main()
