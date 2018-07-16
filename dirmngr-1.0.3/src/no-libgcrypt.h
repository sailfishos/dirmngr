/* no-libgcrypt.h - Replacement functions for libgcrypt.
 *	Copyright (C) 2004 Free Software Foundation, Inc.
 *
 * This file is part of GnuPG.
 *
 * GnuPG is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GnuPG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef NO_LIBGCRYPT_H
#define NO_LIBGCRYPT_H

#define NO_LIBGCRYPT 1

void *gcry_malloc (size_t n);
void *gcry_xmalloc (size_t n);
void *gcry_realloc (void *a, size_t n);
void *gcry_xrealloc (void *a, size_t n);
void *gcry_calloc (size_t n, size_t m);
void *gcry_xcalloc (size_t n, size_t m);
char *gcry_strdup (const char *string);
char *gcry_xstrdup (const char *string);
void gcry_free (void *a);

#endif /*NO_LIBGCRYPT_H*/
