/* misc.h - miscellaneous
 *      Copyright (C) 2002 Klarälvdalens Datakonsult AB
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

#ifndef MISC_H
#define MISC_H

#include <ksba.h>
#include <gcrypt.h>

#include "error.h"

/* A type to hold the ISO time.  Note that this this is the same as
   the the KSBA type ksba_isotime_t. */
typedef char dirmngr_isotime_t[16];

/* Yields the same value as time() but subject to time faking. */
time_t get_time (void); 

/* Return the current time in ISO format. */
void get_isotime (dirmngr_isotime_t timebuf);

/* Set the time; used for faking teh system time. */
void set_time (time_t newtime, int freeze);

/* Returns true when we are in timewarp mode. */
int faked_time_p (void);

/* Check that the 15 bytes in ATIME represent a valid ISO time. */
gpg_error_t check_isotime (const dirmngr_isotime_t atime);

/* Add SECONDS to ATIME. */
gpg_error_t add_isotime (dirmngr_isotime_t atime, int seconds);

/* Copy one ISO date to another, this is inline so that we can do a
   sanity check. */
static inline void
copy_time (dirmngr_isotime_t d, const dirmngr_isotime_t s)
{
  if (*s && (strlen (s) != 15 || s[8] != 'T'))
    BUG ();
  strcpy (d, s);
}

/* Convert hex encoded string back to binary. */
size_t unhexify (unsigned char *result, const char *string);

/* Returns SHA1 hash of the data. */
char* hashify_data( const char* data, size_t len );

/* Returns data as a hex string. */
char* hexify_data( const unsigned char* data, size_t len );

/* Returns the serial number as a hex string.  */
char* serial_hex ( ksba_sexp_t serial );

/* Take an S-Expression encoded blob and return a pointer to the
   actual data as well as its length. */
const unsigned char *serial_to_buffer (const ksba_sexp_t serial,
                                       size_t *length);

/* Do an in-place percent unescaping of STRING. Returns STRING. */
char *unpercent_string (char *string);

gpg_error_t canon_sexp_to_gcry (const unsigned char *canon,
                                gcry_sexp_t *r_sexp);

/* Return an allocated hex-string with the SHA-1 fingerprint of
   CERT. */
char *get_fingerprint_hexstring (ksba_cert_t cert);
/* Return an allocated hex-string with the SHA-1 fingerprint of
   CERT.  This version inserts the usual colons. */
char *get_fingerprint_hexstring_colon (ksba_cert_t cert);

/* Log CERT in short format with s/n and issuer DN prefixed by TEXT.  */
void cert_log_name (const char *text, ksba_cert_t cert);

/* Log CERT in short format with the subject DN prefixed by TEXT.  */
void cert_log_subject (const char *text, ksba_cert_t cert);

/* Dump the serial number SERIALNO to the log stream.  */
void dump_serial (ksba_sexp_t serialno);

/* Dump the ISO time T to the log stream without a LF.  */
void dump_isotime (dirmngr_isotime_t t);

/* Dump STRING to the log file but choose the best readable
   format.  */
void dump_string (const char *string);

/* Dump an KSBA cert object to the log stream. Prefix the output with
   TEXT.  This is used for debugging. */
void dump_cert (const char *text, ksba_cert_t cert);

/* Return the host name and the port (0 if none was given) from the
   URL.  Return NULL on error or if host is not included in the
   URL.  */
char *host_and_port_from_url (const char *url, int *port);


#ifdef HAVE_FOPENCOOKIE
/* We have to implement funopen in terms of glibc's fopencookie. */
FILE *funopen(void *cookie,
              int (*readfn)(void *, char *, int),
              int (*writefn)(void *, const char *, int),
              fpos_t (*seekfn)(void *, fpos_t, int),
              int (*closefn)(void *));
#endif /*HAVE_FOPENCOOKIE*/


#endif /* MISC_H */
