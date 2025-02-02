# SPDX-License-Identifier: BSD-3-Clause

"""
Checker for the "Parallel Graph" assignment.

It walks through the input test files in in/ and compares the serial case
to the parallel case. It adds points and gives out the final result.
"""

import os
import subprocess

TOTAL = 0.0

src = os.environ.get("SRC_PATH", "../src")


def check(testname):
    """Check a test file.

    Pass test filenames `serial` and `parallel` executables.
    """
    with subprocess.Popen([os.path.join(src, "serial"), testname],
            stdout=subprocess.PIPE) as proc_serial_res:
        try:
            outs, _ = proc_serial_res.communicate(timeout=3)
            serial_out = str(outs).strip("\n")
        except subprocess.TimeoutExpired:
            proc_serial_res.kill()
            outs, _ = proc_serial_res.communicate()
            serial_out = ""
    for _ in range(0, 100):
        with subprocess.Popen([os.path.join(src, "parallel"), testname],
                stdout=subprocess.PIPE) as proc_parallel_res:
            try:
                outs, _ = proc_parallel_res.communicate(timeout=3)
                parallel_out = str(outs).strip("\n")
            except subprocess.TimeoutExpired:
                proc_parallel_res.kill()
                outs, _ = proc_parallel_res.communicate()
                parallel_out = ""
            if serial_out != parallel_out:
                return False

    return True


lst = os.listdir("in")
lst.sort(key=lambda s: (len(s), s))
for filename in lst:
    f = os.path.join("in", filename)
    if check(f):
        print(filename.ljust(33) + 23 * "." + " passed ...   4.5")
        TOTAL += 4.5
    else:
        print(filename.ljust(33) + 23 * "." + " failed ...   0.0")

TOTAL = int(TOTAL)
print("\nTotal:" + 61 * " " + f" {TOTAL}/100")