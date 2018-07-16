/* util.h
 *	Copyright (C) 2004, 2008 g10 Code GmbH
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UTIL_H
#define UTIL_H

#include <gpg-error.h>
#ifndef NO_LIBGCRYPT
#include <gcrypt.h>
#endif

/* Get all the stuff from jnlib. */
#include "../jnlib/logging.h"
#include "../jnlib/argparse.h"
#include "../jnlib/stringhelp.h"
#include "../jnlib/mischelp.h"
#include "../jnlib/strlist.h"
#include "../jnlib/dotlock.h"


/*-- b64enc.c and b64dec.c --*/
struct b64state 
{ 
  unsigned int flags;
  int idx;
  int quad_count;
  FILE *fp;
  char *title;
  unsigned char radbuf[4];
  int stop_seen:1;
  int invalid_encoding:1;
};
gpg_error_t b64enc_start (struct b64state *state, FILE *fp, const char *title);
gpg_error_t b64enc_write (struct b64state *state,
                          const void *buffer, size_t nbytes);
gpg_error_t b64enc_finish (struct b64state *state);

gpg_error_t b64dec_start (struct b64state *state, const char *title);
gpg_error_t b64dec_proc (struct b64state *state, void *buffer, size_t length,
                         size_t *r_nbytes);
gpg_error_t b64dec_finish (struct b64state *state);





/* Handy malloc macros - use only them.  */
#define xtrymalloc(a)    gcry_malloc ((a))
#define xtrycalloc(a,b)  gcry_calloc ((a),(b))
#define xtryrealloc(a,b) gcry_realloc ((a),(b))
#define xtrystrdup(a)    gcry_strdup ((a))
#define xfree(a)         gcry_free ((a))

#define xmalloc(a)       gcry_xmalloc ((a))
#define xcalloc(a,b)     gcry_xcalloc ((a),(b))
#define xrealloc(a,b)    gcry_xrealloc ((a),(b))
#define xstrdup(a)       gcry_xstrdup ((a))

/* Macros to replace ctype ones and to avoid locale problems.  */
#define spacep(p)   (*(p) == ' ' || *(p) == '\t')
#define digitp(p)   (*(p) >= '0' && *(p) <= '9')
#define hexdigitp(a) (digitp (a)                     \
                      || (*(a) >= 'A' && *(a) <= 'F')  \
                      || (*(a) >= 'a' && *(a) <= 'f'))
/* These atoi macros assume that the buffer has only valid digits.  */
#define atoi_1(p)   (*(p) - '0' )
#define atoi_2(p)   ((atoi_1(p) * 10) + atoi_1((p)+1))
#define atoi_4(p)   ((atoi_2(p) * 100) + atoi_2((p)+2))
#define xtoi_1(p)   (*(p) <= '9'? (*(p)- '0'): \
                     *(p) <= 'F'? (*(p)-'A'+10):(*(p)-'a'+10))
#define xtoi_2(p)   ((xtoi_1(p) * 16) + xtoi_1((p)+1))


/* Set up the default home directory.  The usual --homedir option
   should be parsed later. */
const char *default_homedir (void);

/* Find the dirmngr_ldap program image.  */
const char *get_dirmngr_ldap_path (void);
const char *dirmngr_sysconfdir (void);
const char *dirmngr_libexecdir (void);
const char *dirmngr_datadir (void);
const char *dirmngr_cachedir (void);
const char *default_socket_name (void);

#endif /*UTIL_H*/
