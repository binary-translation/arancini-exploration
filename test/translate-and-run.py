#! /bin/python3

import os
import sys
import json
import logging
import argparse
import subprocess

# Disable tracebacks
sys.tracebacklimit = 0

logger = logging.getLogger("tester")

class ExecutionError(Exception):
    def __init__(self, command, stdout, stderr):
        self.message = f"Error when executing {command}\nSTDOUT:\n{stdout}\nSTDERR:\n{stderr}"
        super().__init__(self.message)

    def __str__(self):
        return self.message

class Tester:
    def __init__(self, txlat_path, input_bin, config = ""):
        self.artifacts = []
        self.txlat_path = txlat_path
        self.input_bin = input_bin

        if not os.path.isfile(self.txlat_path):
            raise ValueError(f"txlat does not exist at path: {self.txlat_path}")

        if not os.path.isfile(self.input_bin):
            raise ValueError(f"input binary does not exist at path: {self.input_bin}")

        self.config = {
            'compile_flags': [],
            'compile_environment': os.environ.__dict__, # default environment same as tester's
            'runtime_flags': [],
            'runtime_environment': os.environ.__dict__, # default environment same as tester's
            'expected_stdout': None,
            'expected_stderr': None,
            'produced_artifacts': []
        }

        if not os.path.isfile(config):
            logger.warning(f"Config file does not exist at path \"{config}\"; "
                            "executing test in default configuration")
        else:
            self.parse_config(config)
            logger.info(f"Executing test with config:\n{self.config}")

    def run(self):
        logger.info("Translating input binary")

        translated = self.compile()
        self.config["produced_artifacts"].append(translated)

        logger.info(f"Executing transated binary: {translated}")
        stdout, stderr = self.execute(translated)

        self.compare_output(self.config["stdout"], stdout)
        self.compare_output(self.config["stdout"], stdout)

    def __del__(self):
        for output_file in self.config["produced_artifacts"]:
            if os.path.exists(output_file):
                os.remove(output_file)

    def compile(self):
        output_file = self.input_bin + ".out"
        compile_command = [self.txlat_path, "--input", self.input_bin, "--output", output_file,
                           *self.config["compile_flags"]]
        proc = subprocess.run(compile_command, capture_output=True, text=True)
        if proc.returncode != 0:
            raise ExecutionError(compile_command, proc.stdout, proc.stderr)

        return output_file

    def execute(self, binary):
        execute_command = [binary, *self.config["runtime_flags"]]
        proc = subprocess.run(execute_command, capture_output=True, text=True)
        if proc.returncode != 0:
            raise ExecutionError(execute_command, proc.stdout, proc.stderr)

        logger.info("Completed execution successfully")

        return proc.stdout, proc.stderr

    def parse_config(self, config):
        with open(config, 'r') as f:
            config_data = json.load(f)

            for key in self.config.keys():
                if key in config_data:
                    if self.config[key] is None:
                        self.config[key] = config_data[key]
                    elif isinstance(self.config[key], dict):
                        self.config[key].update(config_data[key])
                    else:
                        self.config[key].extend(config_data[key])

    def compare_output(reference, output):
        if reference is not None:
            # TODO
            if reference != output:
                print("Output differs")
                exit(2)

def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.description = 'Arancini binary execution test runner'

    # Specification of the concrete test trace
    parser.add_argument('-i', '--input',
                        required=True,
                        help='Path to input x86 binary to translate and then execute')

    parser.add_argument('-t', '--txlat', '--translator',
                        required=True,
                        help='Path to translator (txlat) program')

    parser.add_argument('-c', '--config',
                        required=False,
                        default="",
                        help='Path to (JSON) configuration file for the tester')

    parser.add_argument('--log-level',
                        required=False,
                        default="WARNING",
                        choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"],
                        help='Logging level for tester')

    return parser.parse_args()

if __name__ == "__main__":
    args = parse_arguments()

    logging.basicConfig(level=getattr(logging, args.log_level))

    try:
        tester = Tester(args.txlat, args.input, args.config)
        tester.run()
    except Exception as e:
        print("Test failed:\n", str(e))
        exit(2)

    exit(0)

