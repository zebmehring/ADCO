#include "cartographer.h"

extern Chp *__chp;
extern bool bundle_data;
extern char *output_file;
extern int optimization;

int main (int argc, char **argv)
{
  Chp *c;
  int chp = 1;

  if (argc > 5)
  {
    fprintf (stderr, "Usage: %s [-h|--help] [-b|--bundle_data] [-O[1]] [-o|--output <flie>] <chp>\n", argv[0]);
    return 1;
  }
  else if (argc > 2)
  {
    chp = argc - 1;
    for (int i = 1; i < chp; i++)
    {
      if (!strcmp (argv[i], "-h") || !strcmp (argv[i], "--help"))
      {
        printf ("Usage: %s [-h|--help] [-b|--bundle_data] [-O[1]] [-o|--output <flie>] <chp>\n\n", argv[0]);
        printf ("Options:\n");
        printf ("  -h, --help\tdisplay this message\n");
        printf ("  -b, --bundle_data\tenable the bundle data protocol for multi-bit expressions\n");
        printf ("  -o, --output <file>\toutput the results to <file>");
        printf ("  -O1\tenable level 1 optimizations\n");
        return 0;
      }
      else if (!strcmp (argv[i], "-b") || !strcmp (argv[i], "--bundle_data"))
      {
        bundle_data = true;
      }
      else if (!strcmp (argv[i], "-o") || !strcmp (argv[i], "--output"))
      {
        output_file = argv[++i];
      }
      else if (!strcmp (argv[i], "-O1"))
      {
        optimization = 1;
      }
      else
      {
        fprintf (stderr, "Usage: %s [-h|--help] [-b|--bundle_data] [-O[1]] [-o|--output <flie>] <chp>\n", argv[0]);
        return 1;
      }
    }
  }
  else if (argc == 2)
  {
    if (!strcmp (argv[1], "-h") || !strcmp (argv[1], "--help"))
    {
      printf ("Usage: %s [-h|--help] [-b|--bundle_data] [-O[1]] [-o|--output <flie>] <chp>\n\n", argv[0]);
      printf ("Options:\n");
      printf ("  -h, --help\tdisplay this message\n");
      printf ("  -b, --bundle_data\tenable the bundle data protocol for multi-bit expressions\n");
      printf ("  -o, --output <file>\toutput the results to <file>");
      printf ("  -O1\tenable level 1 optimizations\n");
      return 0;
    }
  }
  else
  {
    fprintf (stderr, "Usage: %s [-h|--help] [-b|--bundle_data] [-O[1]] [-o|--output <flie>] <chp>\n", argv[0]);
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
