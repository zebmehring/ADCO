# Installation Instructions:

1. Replace the `ACT_PATH` string `#define`'d on line 17 of `~/chp/src/cartographer.c` with the complete path to `~/act`.
2. Replace the arguments to the import statements at the top of `~/act/bundled.act` and `~/act/syn.act` with the complete path to `~/act/<filename>`.
3. Edit the variables defined at the top of all scripts in the `~/scripts` directory to use the path to this directory (`~`). These include: `benchmark.sh`, `chp2prs.sh`, and `test.sh`.
4. Change the macros defined at the top of the `Makefile` to the appropriate paths for the filesystem.

# Usage

* The compiler binary is `~/chp/bin/zcc`. It is made according to the Makefile in `~/chp`.
  * For further information, use the `-h` or `--help` flags, or see the source code.
* Both the compiler and `aflat` can be run in one command using the shell script `~/scripts/chp2prs.sh`.
  * The script accepts all compiler arguments except for the `-o` flag, followed by a list of CHP programs to compile.
  * The script will re-make the compiler binary.
  * ACT files output by the compiler for the program `./[tst | benchmark]/<chp>.chp` are written to `~/act/[tst | benchmark]/[bundled_]<chp>.act`, as if the user had specified `-o ~/act/[tst | benchmark]/[bundled_]<chp>.act` on the command line.
  * Production rules output by `aflat` are written to `~/prs/[tst | benchmark]/[bundled_]<chp>.prs`.
* `prsim` can be run with the commands in `~/scripts/watch_and_run` by invoking `~/scripts/benchmark.sh`, with the desired `.prs` files as arguments.
  * All outputs for `<prs>.prs` are written to `~/results/<prs>.sim`. Transition count dumps are written to `~/results/<prs>.tc`.
* `prsim` can be run with the commands in `~/scripts/watch_and_run_test` by invoking `~/scripts/test.sh`, with the desired `.prs` files as arguments.
  * All outputs for `<prs>.prs` are written to `stdout`.
* `~/scripts/scrape_cycles.py` will extract the first integer from the penultimate line of any `.sim` files passed to it as arguments.
  * In the case of a successful simulation using `benchmark.sh`, this should be the cycle of the final transition in the simulation.
  * The results are written to the file `~/results/cycle_totals`.
* `~/scripts/scrape_tcs.py` will accumulate the last integer on each line from any `.tc` files passed to it as arguments.
  * In the case of a successful simulation using `benchmark.sh`, this should be the total number of transitions in the simulation.
  * The results are written to the file `~/results/tc_totals`.
