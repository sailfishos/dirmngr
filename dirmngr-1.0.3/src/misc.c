/* misc.c - miscellaneous
 *	Copyright (C) 2002 Klarälvdalens Datakonsult AB
 *      Copyright (C) 2002, 2003, 2004 Free Software Foundation, Inc.
 *
 * This file is part of DirMngr.
 *
 * DirMngr is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * DirMngr is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "util.h"
#include "misc.h"
#include "dirmngr.h"

static unsigned long timewarp;
static enum { NORMAL = 0, FROZEN, FUTURE, PAST } timemode;

/* Wrapper for the time(3).  We use this here so we can fake the time
   for tests. */
time_t 
get_time () 
{
  time_t current = time (NULL);
  if (timemode == NORMAL)
    return current;
  else if (timemode == FROZEN)
    return timewarp;
  else if (timemode == FUTURE)
    return current + timewarp;
  else
    return current - timewarp;
}



/* Return the current time in ISO format. */
void
get_isotime (dirmngr_isotime_t timebuf)
{
  time_t atime = get_time ();
    
  if (atime < 0)
    *timebuf = 0;
  else 
    {
      struct tm *tp;
#ifdef HAVE_GMTIME_R
      struct tm tmbuf;
      
      tp = gmtime_r (&atime, &tmbuf);
#else
      tp = gmtime (&atime);
#endif
      sprintf (timebuf,"%04d%02d%02dT%02d%02d%02d",
               1900 + tp->tm_year, tp->tm_mon+1, tp->tm_mday,
               tp->tm_hour, tp->tm_min, tp->tm_sec);
    }
}


/* Set the time to NEWTIME so that get_time returns a time starting
   with this one.  With FREEZE set to 1 the returned time will never
   change.  Just for completeness, a value of (time_t)-1 for NEWTIME
   gets you back to reality. Note that this is obviously not
   thread-safe but this is not required. */
void
set_time (time_t newtime, int freeze)
{
  time_t current = time (NULL);

  if ( newtime == (time_t)-1 || current == newtime)
    {
      timemode = NORMAL;
      timewarp = 0;
    }
  else if (freeze)
    {
      timemode = FROZEN;
      timewarp = current;
    }
  else if (newtime > current)
    {
      timemode = FUTURE;
      timewarp = newtime - current;
    }
  else
    {
      timemode = PAST;
      timewarp = current - newtime;
    }
}

/* Returns true when we are in timewarp mode. */
int
faked_time_p (void)
{
  return timemode;
}




/* Check that the 15 bytes in ATIME represent a valid ISO time.  Note
   that this function does not expect a string but a plain 15 byte
   isotime buffer. */
gpg_error_t
check_isotime (const dirmngr_isotime_t atime)
{
  int i;
  const char *s;

  if (!*atime)
    return gpg_error (GPG_ERR_NO_VALUE);
  
  for (s=atime, i=0; i < 8; i++, s++)
    if (!digitp (s))
      return gpg_error (GPG_ERR_INV_TIME);
  if (*s != 'T')
      return gpg_error (GPG_ERR_INV_TIME);
  for (s++, i=9; i < 15; i++, s++)
    if (!digitp (s))
      return gpg_error (GPG_ERR_INV_TIME);
  return 0;
}


/* Correction used to map to real Julian days. */
#define JD_DIFF 1721060L

static int
days_per_year (int y)
{
  int s ;

  s = !(y % 4);
  if ( !(y % 100))
    if ((y%400))
      s = 0;
  return s ? 366 : 365;
}

static int
days_per_month (int y, int m)
{
  int s;
    
  switch(m)
    {
    case 1: case 3: case 5: case 7: case 8: case 10: case 12:
      return 31 ;
    case 2:
      s = !(y % 4);
      if (!(y % 100))
        if ((y % 400))
          s = 0;
      return s? 29 : 28 ;
    case 4: case 6: case 9: case 11:
      return 30;
    }
  BUG();
}


/* Convert YEAR, MONTH and DAY into the Julian date.  We assume that
   it is already noon; we dont; support dates before 1582-10-15. */
static unsigned long
date2jd (int year, int month, int day)
{
  unsigned long jd;

  jd = 365L * year + 31 * (month-1) + day + JD_DIFF;
  if (month < 3)
    year-- ;
  else
    jd -= (4 * month + 23) / 10;

  jd += year / 4 - ((year / 100 + 1) *3) / 4;

  return jd ;
}

/* Convert a Julian date back to YEAR, MONTH and DAY.  Return day of
   the year or 0 on error.  This function uses some more or less
   arbitrary limits, most important is that days before 1582 are not
   supported. */
static int
jd2date (unsigned long jd, int *year, int *month, int *day)
{
  int y, m, d;
  long delta;

  if (!jd)
    return 0 ;
  if (jd < 1721425 || jd > 2843085)
    return 0;

  y = (jd - JD_DIFF) / 366;
  d = m = 1;

  while ((delta = jd - date2jd (y, m, d)) > days_per_year (y))
    y++;

  m = (delta / 31) + 1;
  while( (delta = jd - date2jd (y, m, d)) > days_per_month (y,m))
    if (++m > 12)
      { 
        m = 1;
        y++;
      }

  d = delta + 1 ;
  if (d > days_per_month (y, m))
    { 
      d = 1;
      m++;
    }
  if (m > 12)
    { 
      m = 1;
      y++;
    }

  if (year)
    *year = y;
  if (month)
    *month = m;
  if (day)
    *day = d ;

  return (jd - date2jd (y, 1, 1)) + 1;
}




/* Add SECONDS to ATIME.  SECONDS may not be negative and is limited
   to about the equivalent of 62 years which should be more then
   enough for our purposes. */
gpg_error_t
add_isotime (dirmngr_isotime_t atime, int seconds )
{
  gpg_error_t err;
  int year, month, day, hour, minute, sec, ndays;
  unsigned long jd;

  err = check_isotime (atime);
  if (err)
    return err;

  if (seconds < 0 || seconds >= (0x7fffffff - 61) )
    return gpg_error (GPG_ERR_INV_VALUE);

  year  = atoi_4 (atime+0);
  month = atoi_2 (atime+4);
  day   = atoi_2 (atime+6);
  hour  = atoi_2 (atime+9);
  minute= atoi_2 (atime+11);
  sec   = atoi_2 (atime+13);

  if (year <= 1582) /* The julian date functions don't support this. */
    return gpg_error (GPG_ERR_INV_VALUE); 

  sec    += seconds;
  minute += sec/60;
  sec    %= 60;
  hour   += minute/60;
  minute %= 60;
  ndays  = hour/24;
  hour   %= 24;
  
  jd = date2jd (year, month, day) + ndays;
  jd2date (jd, &year, &month, &day);

  if (year > 9999 || month > 12 || day > 31
      || year < 0 || month < 1 || day < 1)
    return gpg_error (GPG_ERR_INV_VALUE); 
    
  sprintf (atime, "%04d%02d%02dT%02d%02d%02d",
           year, month, day, hour, minute, sec);
  return 0;
}




/* Convert the hex encoded STRING back into binary and store the
   result into the provided buffer RESULT.  The actual size of that
   buffer will be returned.  The caller should provide RESULT of at
   least strlen(STRING)/2 bytes.  There is no error detection, the
   parsing stops at the first non hex character.  With RESULT given as
   NULL, the fucntion does only return the size of the buffer which
   would be needed.  */
size_t
unhexify (unsigned char *result, const char *string)
{
  const char *s;
  size_t n;

  for (s=string,n=0; hexdigitp (s) && hexdigitp(s+1); s += 2)
    {
      if (result)
        result[n] = xtoi_2 (s);
      n++;
    }
  return n;
}


char* 
hashify_data( const char* data, size_t len )
{
  unsigned char buf[20];
  gcry_md_hash_buffer (GCRY_MD_SHA1, buf, data, len);
  return hexify_data( buf, 20 );
}

char* 
hexify_data( const unsigned char* data, size_t len )
{
  int i;
  char* result = xmalloc( sizeof( char ) * (2*len+1));

  for( i = 0; i < 2*len; i+=2 )
    sprintf( result+i, "%02X", *data++);
  return result;
}

char *
serial_hex (ksba_sexp_t serial )
{
  unsigned char* p = serial;
  char *endp;
  unsigned long n;
  char *certid;

  if (!p)
    return NULL;
  else {
    p++; /* ignore initial '(' */
    n = strtoul (p, (char**)&endp, 10);
    p = endp;
    if (*p!=':')
      return NULL;
    else {
      int i = 0;
      certid = xmalloc( sizeof( char )*(2*n + 1 ) );
      for (p++; n; n--, p++) {
	sprintf ( certid+i , "%02X", *p);
	i += 2;
      }
    }
  }
  return certid;
}


/* Take an S-Expression encoded blob and return a pointer to the
   actual data as well as its length.  Return NULL for an invalid
   S-Expression.*/
const unsigned char *
serial_to_buffer (const ksba_sexp_t serial, size_t *length)
{
  unsigned char *p = serial;
  char *endp;
  unsigned long n;

  if (!p || *p != '(')
    return NULL;
  p++;
  n = strtoul (p, &endp, 10);
  p = endp;
  if (*p != ':')
    return NULL;
  p++;
  *length = n;
  return p;
}


/* Do an in-place percent unescaping of STRING. Returns STRING. Noet
   that this function does not do a '+'-to-space unescaping.*/
char *
unpercent_string (char *string)
{
  char *s = string;
  char *d = string;

  while (*s)
    {
      if (*s == '%' && s[1] && s[2])
        { 
          s++;
          *d++ = xtoi_2 ( s);
          s += 2;
        }
      else
        *d++ = *s++;
    }
  *d = 0; 
  return string;
}

/* Convert a canonical encoded S-expression in CANON into the GCRY
   type. */
gpg_error_t
canon_sexp_to_gcry (const unsigned char *canon, gcry_sexp_t *r_sexp)
{
  gpg_error_t err;
  size_t n;
  gcry_sexp_t sexp;

  *r_sexp = NULL;
  n = gcry_sexp_canon_len (canon, 0, NULL, NULL);
  if (!n) 
    {
      log_error (_("invalid canonical S-expression found\n"));
      err = gpg_error (GPG_ERR_INV_SEXP);
    }
  else if ((err = gcry_sexp_sscan (&sexp, NULL, canon, n)))
    log_error (_("converting S-expression failed: %s\n"), gcry_strerror (err));
  else
    *r_sexp = sexp;
  return err;
}


/* Return an allocated buffer with the formatted fingerprint as one
   large hexnumber */
char *
get_fingerprint_hexstring (ksba_cert_t cert)
{
  unsigned char digest[20];
  gcry_md_hd_t md;
  int rc;
  char *buf;
  int i;

  rc = gcry_md_open (&md, GCRY_MD_SHA1, 0);
  if (rc)
    log_fatal (_("gcry_md_open failed: %s\n"), gpg_strerror (rc));

  rc = ksba_cert_hash (cert, 0, HASH_FNC, md);
  if (rc)
    {
      log_error (_("oops: ksba_cert_hash failed: %s\n"), gpg_strerror (rc));
      memset (digest, 0xff, 20); /* Use a dummy value. */
    }
  else
    {
      gcry_md_final (md);
      memcpy (digest, gcry_md_read (md, GCRY_MD_SHA1), 20);
    }
  gcry_md_close (md);
  buf = xmalloc (41);
  *buf = 0;
  for (i=0; i < 20; i++ )
    sprintf (buf+strlen(buf), "%02X", digest[i]);
  return buf;
}

/* Return an allocated buffer with the formatted fingerprint as one
   large hexnumber.  This version inserts the usual colons. */
char *
get_fingerprint_hexstring_colon (ksba_cert_t cert)
{
  unsigned char digest[20];
  gcry_md_hd_t md;
  int rc;
  char *buf;
  int i;

  rc = gcry_md_open (&md, GCRY_MD_SHA1, 0);
  if (rc)
    log_fatal (_("gcry_md_open failed: %s\n"), gpg_strerror (rc));

  rc = ksba_cert_hash (cert, 0, HASH_FNC, md);
  if (rc)
    {
      log_error (_("oops: ksba_cert_hash failed: %s\n"), gpg_strerror (rc));
      memset (digest, 0xff, 20); /* Use a dummy value. */
    }
  else
    {
      gcry_md_final (md);
      memcpy (digest, gcry_md_read (md, GCRY_MD_SHA1), 20);
    }
  gcry_md_close (md);
  buf = xmalloc (61);
  *buf = 0;
  for (i=0; i < 20; i++ )
    sprintf (buf+strlen(buf), "%02X:", digest[i]);
  buf[strlen(buf)-1] = 0; /* Remove railing colon. */
  return buf;
}


/* Dump the serial number SERIALNO to the log stream.  */
void
dump_serial (ksba_sexp_t serialno)
{
  char *p;

  p = serial_hex (serialno);
  log_printf ("%s", p?p:"?");
  xfree (p);
}


/* Dump the ISO time T to the log stream without a LF.  */
void
dump_isotime (dirmngr_isotime_t t)
{
  if (!t || !*t)
    log_printf (_("[none]"));
  else
    log_printf ("%.4s-%.2s-%.2s %.2s:%.2s:%s",
                t, t+4, t+6, t+9, t+11, t+13);
}


/* Dump STRING to the log file but choose the best readable
   format.  */
void
dump_string (const char *string)
{

  if (!string)
    log_printf ("[error]");
  else
    {
      const unsigned char *s;

      for (s=string; *s; s++)
        {
          if (*s < ' ' || (*s >= 0x7f && *s <= 0xa0))
            break;
        }
      if (!*s && *string != '[')
        log_printf ("%s", string);
      else
        {
          log_printf ( "[ ");
          log_printhex (NULL, string, strlen (string));
          log_printf ( " ]");
        }
    }
}

/* Dump an KSBA cert object to the log stream. Prefix the output with
   TEXT.  This is used for debugging. */
void 
dump_cert (const char *text, ksba_cert_t cert)
{
  ksba_sexp_t sexp;
  char *p;
  ksba_isotime_t t;

  log_debug ("BEGIN Certificate `%s':\n", text? text:"");
  if (cert)
    {
      sexp = ksba_cert_get_serial (cert);
      p = serial_hex (sexp);
      log_debug ("     serial: %s\n", p?p:"?");
      xfree (p);
      ksba_free (sexp);

      ksba_cert_get_validity (cert, 0, t);
      log_debug ("  notBefore: ");
      dump_isotime (t);
      log_printf ("\n");
      ksba_cert_get_validity (cert, 1, t);
      log_debug ("   notAfter: ");
      dump_isotime (t);
      log_printf ("\n");

      p = ksba_cert_get_issuer (cert, 0);
      log_debug ("     issuer: ");
      dump_string (p);
      ksba_free (p);
      log_printf ("\n");
    
      p = ksba_cert_get_subject (cert, 0);
      log_debug ("    subject: ");
      dump_string (p);
      ksba_free (p);
      log_printf ("\n");

      log_debug ("  hash algo: %s\n", ksba_cert_get_digest_algo (cert));

      p = get_fingerprint_hexstring (cert);
      log_debug ("  SHA1 fingerprint: %s\n", p);
      xfree (p);
    }
  log_debug ("END Certificate\n");
}



/* Log the certificate's name in "#SN/ISSUERDN" format along with
   TEXT. */
void 
cert_log_name (const char *text, ksba_cert_t cert)
{
  log_info ("%s", text? text:"certificate" );
  if (cert)
    {
      ksba_sexp_t sn;
      char *p;

      p = ksba_cert_get_issuer (cert, 0);
      sn = ksba_cert_get_serial (cert);
      if (p && sn)
        {
          log_printf (" #");
          dump_serial (sn);
          log_printf ("/");
          dump_string (p);
        }
      else
        log_printf (" [invalid]");
      ksba_free (sn);
      xfree (p);
    }
  log_printf ("\n");
}


/* Log the certificate's subject DN along with TEXT. */
void 
cert_log_subject (const char *text, ksba_cert_t cert)
{
  log_info ("%s", text? text:"subject" );
  if (cert)
    {
      char *p;

      p = ksba_cert_get_subject (cert, 0);
      if (p)
        {
          log_printf (" /");
          dump_string (p);
          xfree (p);
        }
      else
        log_printf (" [invalid]");
    }
  log_printf ("\n");
}


/****************
 * Remove all %xx escapes; this is done inplace.
 * Returns: New length of the string.
 */
static int
remove_percent_escapes (unsigned char *string)
{
  int n = 0;
  unsigned char *p, *s;

  for (p = s = string; *s; s++)
    {
      if (*s == '%')
        {
          if (s[1] && s[2] && hexdigitp (s+1) && hexdigitp (s+2))
            {
              s++;
              *p = xtoi_2 (s);
              s++;
              p++;
              n++;
            }
          else
            {
              *p++ = *s++;
              if (*s)
                *p++ = *s++;
              if (*s)
                *p++ = *s++;
              if (*s)
                *p = 0;
              return -1;   /* Bad URI. */
            }
        }
      else
        {
          *p++ = *s;
          n++;
        }
    }
  *p = 0;  /* Always keep a string terminator. */
  return n;
}


/* Return the host name and the port (0 if none was given) from the
   URL.  Return NULL on error or if host is not included in the
   URL.  */
char *
host_and_port_from_url (const char *url, int *port)
{
  const char *s, *s2;
  char *buf, *p;
  int n;

  s = url;

  *port = 0;

  /* Find the scheme */
  if ( !(s2 = strchr (s, ':')) || s2 == s )
    return NULL;  /* No scheme given. */
  s = s2+1;

  /* Find the hostname */
  if (*s != '/')
    return NULL; /* Does not start with a slash. */

  s++;
  if (*s != '/')
    return NULL; /* No host name.  */
  s++;

  buf = xtrystrdup (s);
  if (!buf)
    {
      log_error (_("malloc failed: %s\n"), strerror (errno));
      return NULL;
    }
  if ((p = strchr (buf, '/')))
    *p++ = 0;
  strlwr (buf);
  if ((p = strchr (p, ':')))
    {
      *p++ = 0;
      *port = atoi (p);
    }

  /* Remove quotes and make sure that no Nul has been encoded. */
  if ((n = remove_percent_escapes (buf)) < 0
      || n != strlen (buf) )
    {
      log_error (_("bad URL encoding detected\n"));
      xfree (buf);
      return NULL;
    }

  return buf;
}

