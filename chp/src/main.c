#include "cartographer.h"

extern Chp *__chp;

int main (int argc, char **argv)
{
  Chp *c;

  if (argc != 2)
  {
    fprintf (stderr, "Usage: %s <chp>\n", argv[0]);
    return 1;
  }

  c = read_chp (argv[1]);

  /* check that variables are used properly */
  __chp = c;
  check_types (c);
  /* print the CHP program */
  print_chp_structure (c);

  return 0;
}
