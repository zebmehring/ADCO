#include "cartographer.h"

extern Chp *__chp;
extern bool bundle_data;

int main (int argc, char **argv)
{
  Chp *c;
  int chp = 1;

  if (argc > 3)
  {
    fprintf (stderr, "Usage: %s [-b] <chp>\n", argv[0]);
    return 1;
  }
  else if (argc == 3)
  {
    chp = 2;
    if (strcmp(argv[1], "-b"))
    {
      fprintf (stderr, "Usage: %s [-b] <chp>\n", argv[0]);
      return 1;
    }
    bundle_data = true;
  }

  c = read_chp (argv[chp]);

  /* check that variables are used properly */
  __chp = c;
  check_types (c);
  /* print the CHP program */
  print_chp_structure (c);

  return 0;
}
