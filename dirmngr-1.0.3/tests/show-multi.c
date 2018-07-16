/* show-multi.c  -  test tool to show dirmngr_ldap --multi output
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

#define PGM "show-multi"

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
copy_n_bytes (FILE *fp, size_t n)
{
  int c;

  while (n-- && (c=getc(fp))!=EOF)
    if ( c == '\\' )
      fputs ("\\\\", stdout);
    else if ( c == '\n' )
      fputs ("\\n", stdout);
    else if ( c == '\r' )
      fputs ("\\r", stdout);
    else if ( c == '\t' )
      fputs ("\\t", stdout);
    else if ( c == '\f' )
      fputs ("\\f", stdout);
    else if ( c == '\v' )
      fputs ("\\v", stdout);
    else if ( c == '\b' )
      fputs ("\\b", stdout);
    else if ( c == '\a' )
      fputs ("\\a", stdout);
    else if (c < 0x20 || c >= 0x7f)
      printf ("\\x%02X", (unsigned int)c);
    else
      putchar (c);
  
}



static void
show_data (FILE *fp)
{
  unsigned char hdr[5];
  size_t length;
  int n;


  while ((n=fread (hdr, 1, 5, fp)) == 5)
    {
      length = (hdr[1] << 24)|(hdr[2]<<16)|(hdr[3]<<8)|hdr[4];
      if (*hdr == 'I')
        {
          printf ("ITEM\n");
          if (length)
            die ("header type 'I' with a non-zero length\n");
        }
      else if (*hdr == 'A')
        {
          printf ("  ATTRIBUTE ");
          copy_n_bytes (fp, length);
          putchar ('\n');
        }
      else if (*hdr == 'V')    
        {
          printf ("      VALUE ");
          copy_n_bytes (fp, length);
          putchar ('\n');
        }
      else
        die ("invalid header type 0x%02X\n", *hdr);
    }
  if (ferror (fp))
    die ("error reading input: %s\n", strerror (errno));
  else if (n)
    die ("header record too short\n");
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
                "Show the output of dirmngr_ldap --multi\n\n"
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
      show_data (fp);
      fclose (fp);
    }
  else
    show_data (stdin);

  return 0;
}


/*
Local Variables:
compile-command: "gcc -Wall -g -o show-multi show-multi.c"
End:
*/
