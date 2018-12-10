#!/usr/bin/python

import re, sys, os

def reduce(tcs):
    count = 0
    for line in tcs:
        m = re.match(r".*\s(\d+)", line)
        if m != None:
            count += int(m.group(1))
    return count

def main():
    output = "/home/user/Documents/ADCO/results/tc_totals"
    if os.path.isfile(output):
        os.remove(output)
    for file in sys.argv[1:]:
        if re.match(r".+\.tc$", file) == None:
            continue
        with open(file) as f:
            tcs = f.readlines()
            total_count = reduce(tcs)
        m = re.match(r"(.*\/)*(\w+)\.tc", file)
        if m != None:
            with open(output, "a+") as f:
                f.write(m.group(2) + ": " + str(total_count) + "\n")

if __name__ == "__main__":
	main()
