#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "chp.h"
#include "check_types.h"
#include "cartographer.h"

extern Chp *__chp;
extern bool bundle_data;
extern bool benchmark;
extern char *output_file;
extern int optimization;

/*
 * Summary: :)
 */
void print_smiley ()
{
  fprintf (stdout, "/*************************************************************\n");
  fprintf (stdout, "*                     *****************                      *\n");
  fprintf (stdout, "*                ******               ******                 *\n");
  fprintf (stdout, "*            ****                           ****             *\n");
  fprintf (stdout, "*         ****                                 ***           *\n");
  fprintf (stdout, "*       ***                                       ***        *\n");
  fprintf (stdout, "*      **           ***               ***           **       *\n");
  fprintf (stdout, "*    **           *******           *******          ***     *\n");
  fprintf (stdout, "*   **            *******           *******            **    *\n");
  fprintf (stdout, "*  **             *******           *******             **   *\n");
  fprintf (stdout, "*  **               ***               ***               **   *\n");
  fprintf (stdout, "* **                                                     **  *\n");
  fprintf (stdout, "* **       *                                     *       **  *\n");
  fprintf (stdout, "* **      **                                     **      **  *\n");
  fprintf (stdout, "*  **   ****                                     ****   **   *\n");
  fprintf (stdout, "*  **      **                                   **      **   *\n");
  fprintf (stdout, "*   **       ***                             ***       **    *\n");
  fprintf (stdout, "*    ***       ****                       ****       ***     *\n");
  fprintf (stdout, "*      **         ******             ******         **       *\n");
  fprintf (stdout, "*       ***            ***************            ***        *\n");
  fprintf (stdout, "*         ****                                 ****          *\n");
  fprintf (stdout, "*            ****                           ****             *\n");
  fprintf (stdout, "*                ******               ******                 *\n");
  fprintf (stdout, "*                     *****************                      *\n");
  fprintf (stdout, "*************************************************************/\n");
}

int main (int argc, char **argv)
{
  Chp *c;
  int chp = 1;
  bool smiley = false;

  if (argc > 6)
  {
    fprintf (stderr, "Usage: %s [-h | --help] [[-b | --bundle_data] | [-O[1]]] [-o | --output <file>] [-s | --smiley] <chp>\n", argv[0]);
    return 1;
  }
  else if (argc > 2)
  {
    chp = argc - 1;
    for (int i = 1; i < chp; i++)
    {
      if (!strcmp (argv[i], "-h") || !strcmp (argv[i], "--help"))
      {
        printf ("Usage: %s [-h | --help] [[-b | --bundle_data] | [-O[1]]] [-o | --output <file>] [-s | --smiley] <chp>\n\n", argv[0]);
        printf ("Options:\n");
        printf ("  -h, --help\tdisplay this message\n");
        printf ("  -b, --bundle_data\tenable the bundle data protocol for multi-bit expressions\n");
        printf ("  -o, --output <file>\toutput the results to <file>");
        printf ("  -O1\tenable level 1 optimizations (global common expression elimination for parallel constructs)\n");
        return 0;
      }
      else if (!strcmp (argv[i], "-b") || !strcmp (argv[i], "--bundle_data"))
      {
        if (optimization) fprintf (stderr, "Warning: bundle_data not compatible with optimization\n");
        bundle_data = true;
        optimization = 0;
      }
      else if (!strcmp (argv[i], "-o") || !strcmp (argv[i], "--output"))
      {
        output_file = argv[++i];
      }
      else if (!strcmp (argv[i], "-O1"))
      {
        if (bundle_data) fprintf (stderr, "Warning: optimization not compatible with bundle_data\n");
        optimization = 1;
        bundle_data = false;
      }
      else if (!strcmp (argv[i], "--smiley") || !strcmp (argv[i], "-s"))
      {
        smiley = true;
      }
      else
      {
        fprintf (stderr, "Usage: %s [-h | --help] [[-b | --bundle_data] | [-O[1]]] [-o | --output <file>] [-s | --smiley] <chp>\n", argv[0]);
        return 1;
      }
    }
  }
  else if (argc == 2)
  {
    if (!strcmp (argv[1], "-h") || !strcmp (argv[1], "--help"))
    {
      printf ("Usage: %s [-h | --help] [[-b | --bundle_data] | [-O[1]]] [-o | --output <file>] [-s | --smiley] <chp>\n\n", argv[0]);
      printf ("Options:\n");
      printf ("  -h, --help\tdisplay this message\n");
      printf ("  -b, --bundle_data\tenable the bundle data protocol for multi-bit expressions\n");
      printf ("  -o, --output <file>\toutput the results to <file>");
      printf ("  -O1\tenable level 1 optimizations (global common expression elimination for parallel constructs)\n");
      return 0;
    }
  }
  else
  {
    fprintf (stderr, "Usage: %s [-h | --help] [[-b | --bundle_data] | [-O[1]]] [-o | --output <file>] [-s | --smiley] <chp>\n", argv[0]);
    return 1;
  }

  // signal output to a different directory in the case of benchmarking programs
  if (strstr (argv[chp], "benchmarks")) benchmark = true;
  // parse the CHP program
  c = read_chp (argv[chp]);
  // check that variables are used properly
  __chp = c;
  check_types (c);
  // print the CHP program
  print_chp_structure (c);
  // :)
  if (smiley) print_smiley();

  return 0;
}
