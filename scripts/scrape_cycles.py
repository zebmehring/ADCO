#!/usr/bin/python

import re, sys, os

def get_last_cycle(transitions):
    m = re.match(r"^\s*(\d+).*", transitions[-2])
    if m != None:
        return m.group(1)
    else:
        return None

def main():
    output = "/home/user/Documents/ADCO/results/cycle_totals"
    if os.path.isfile(output):
        os.remove(output)
    for file in sys.argv[1:]:
        if re.match(r".+\.sim$", file) == None:
            continue
        with open(file) as f:
            lines = f.readlines()
            total = get_last_cycle(lines)
        m = re.match(r"(.*\/)*(\w+)\.sim", file)
        if m != None and total != None:
            with open(output, "a+") as f:
                f.write(m.group(2) + ": " + str(total) + "\n")

if __name__ == "__main__":
	main()
