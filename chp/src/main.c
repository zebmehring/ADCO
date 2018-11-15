#include "cartographer.h"

extern Chp *__chp;
extern bool bundle_data;

int main (int argc, char **argv)
{
  Chp *c;
  int chp = 1;

  if (argc > 3)
  {
    fprintf (stderr, "Usage: %s [-b|--bundle_data] <chp>\n", argv[0]);
    return 1;
  }
  else if (argc == 3)
  {
    if (!strcmp (argv[1], "-h") || !strcmp (argv[1], "--help"))
    {
      printf("Usage: %s [-b|--bundle_data] <chp>\n\n", argv[0]);
      printf("Options:\n");
      printf("  -b, --bundle_data\tenable the bundle data protocol for multi-bit expressions\n");
      return 0;
    }
    else if (strcmp (argv[1], "-b") && strcmp (argv[1], "--bundle_data"))
    {
      fprintf (stderr, "Usage: %s [-b|--bundle_data] <chp>\n", argv[0]);
      return 1;
    }
    else
    {
      chp = 2;
      bundle_data = true;
    }
  }
  else if (argc == 2)
  {
    if (!strcmp (argv[1], "-h") || !strcmp (argv[1], "--help"))
    {
      printf("Usage: %s [-b|--bundle_data] <chp>\n\n", argv[0]);
      printf("Options:\n");
      printf("  -b, --bundle_data\tenable the bundle data protocol for multi-bit expressions\n");
      return 0;
    }
    else
    {
      fprintf (stderr, "Usage: %s [-b|--bundle_data] <chp>\n", argv[0]);
      return 1;
    }
  }
  else
  {
    fprintf (stderr, "Usage: %s [-b|--bundle_data] <chp>\n", argv[0]);
    return 1;
  }

  c = read_chp (argv[chp]);

  /* check that variables are used properly */
  __chp = c;
  check_types (c);
  /* print the CHP program */
  print_chp_structure (c);

  return 0;
}
