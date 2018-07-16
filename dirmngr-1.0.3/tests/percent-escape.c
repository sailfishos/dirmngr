/*percent-escape.c  - Test tool to create percent escaped data
 *	Copyright (C) 2004 g10 Code GmbH
 *
 * This file is part of DirMngr.
 *
 * DirMngr is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * DirMngr is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#define PGM "percent-escape"

/* Print diagnostic message and exit with failure. */
static void
die (const char *format, ...)
{
  va_list arg_ptr;

  fflush (stdout);
  fprintf (stderr, "%s: ", PGM);

  va_start (arg_ptr, format);
  vfprintf (stderr, format, arg_ptr);
  va_end (arg_ptr);
  putc ('\n', stderr);

  exit (1);
}


static void
proc (FILE *fp)
{
  int c;

  while ((c=getc (fp))!= EOF)
    if ( c <= ' ' || c >= 127 || c == '%' )
      printf ("%%%02X", (unsigned int)c);
    else
      putchar (c);
}



int 
main (int argc, char **argv)
{
  int last_argc = -1;
 
  if (argc)
    {
      argc--; argv++;
    }
  while (argc && last_argc != argc )
    {
      last_argc = argc;
      if (!strcmp (*argv, "--"))
        {
          argc--; argv++;
          break;
        }
      else if (!strcmp (*argv, "--help"))
        {
          puts (
                "Usage: " PGM " [OPTION] [FILE]\n"
                "Percent escape the input\n\n"
                "  --help      display this help and exit\n\n"
                "With no FILE, or when FILE is -, read standard input.\n");
          exit (0);
        }
    }          
 
  if (argc > 1)
    die ("usage: " PGM " [OPTION] [FILE] (try --help for more information)\n");

  if (argc && strcmp (*argv, "-"))
    {
      FILE *fp = fopen (*argv, "rb");
      if (!fp)
        die ("can't open `%s': %s", *argv, strerror (errno));
      proc (fp);
      fclose (fp);
    }
  else
    proc (stdin);

  return 0;
}


/*
Local Variables:
compile-command: "gcc -Wall -g -o percent-escape percent-escape.c"
End:
*/
