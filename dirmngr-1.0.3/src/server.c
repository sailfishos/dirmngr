/* dirmngr.c - LDAP access
 *	Copyright (C) 2002 Klarälvdalens Datakonsult AB
 *      Copyright (C) 2003, 2004, 2005, 2007, 2008 g10 Code GmbH
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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <assuan.h> /* needed for the malloc hooks */


#define JNLIB_NEED_LOG_LOGV
#include "crlcache.h"
#include "crlfetch.h"
#include "dirmngr.h"
#include "ldapserver.h"
#include "ocsp.h"
#include "certcache.h"
#include "validate.h"
#include "misc.h"

/* To avoid DoS attacks we limit the size of a certificate to
   something reasonable. */
#define MAX_CERT_LENGTH (8*1024)

#define PARM_ERROR(t) assuan_set_error (ctx, \
                                        gpg_error (GPG_ERR_ASS_PARAMETER), (t))



/* Control structure per connection. */
struct server_local_s 
{
  /* Data used to associate an Assuan context with local server data */
  assuan_context_t assuan_ctx;

  /* Per-session LDAP serfver.  */
  ldap_server_t ldapservers;
};




/* Accessor for the local ldapservers variable. */
ldap_server_t
get_ldapservers_from_ctrl (ctrl_t ctrl)
{
  if (ctrl && ctrl->server_local)
    return ctrl->server_local->ldapservers;
  else
    return NULL;
}



/* Copy the % and + escaped string S into the buffer D and replace the
   escape sequences.  Note, that it is sufficient to allocate the
   target string D as long as the source string S, i.e.: strlen(s)+1.
   NOte further that If S contains an escaped binary nul the resulting
   string D will contain the 0 as well as all other characters but it
   will be impossible to know whether this is the original EOS or a
   copied Nul. */
static void
strcpy_escaped_plus (char *d, const unsigned char *s)
{
  while (*s)
    {
      if (*s == '%' && s[1] && s[2])
        {
          s++;
          *d++ = xtoi_2 ( s);
          s += 2;
        }
      else if (*s == '+')
        *d++ = ' ', s++;
      else
        *d++ = *s++;
    }
  *d = 0;
}


/* Check whether the option NAME appears in LINE */
static int
has_option (const char *line, const char *name)
{
  const char *s;
  int n = strlen (name);

  s = strstr (line, name);
  return (s && (s == line || spacep (s-1)) && (!s[n] || spacep (s+n)));
}

/* Same as has_option but only considers options at the begin of the
   line.  This is useful for commands which allow arbitrary strings on
   the line.  */
static int
has_leading_option (const char *line, const char *name)
{
  const char *s;
  int n;

  if (name[0] != '-' || name[1] != '-' || !name[2] || spacep (name+2))
    return 0;
  n = strlen (name);
  while ( *line == '-' && line[1] == '-' )
    {
      s = line;
      while (*line && !spacep (line))
        line++;
      if (n == (line - s) && !strncmp (s, name, n))
        return 1;
      while (spacep (line))
        line++;
    }
  return 0;
}


/* Same as has_option but does only test for the name of the option
   and ignores an argument, i.e. with NAME being "--hash" it would
   return a pointer for "--hash" as well as for "--hash=foo".  If
   thhere is no such option NULL is returned.  The pointer returned
   points right behind the option name, this may be an equal sign, Nul
   or a space.  */
/* static const char * */
/* has_option_name (const char *line, const char *name) */
/* { */
/*   const char *s; */
/*   int n = strlen (name); */

/*   s = strstr (line, name); */
/*   return (s && (s == line || spacep (s-1)) */
/*           && (!s[n] || spacep (s+n) || s[n] == '=')) ? (s+n) : NULL; */
/* } */


/* Skip over options.  It is assumed that leading spaces have been
   removed (this is the case for lines passed to a handler from
   assuan).  Blanks after the options are also removed. */
static char *
skip_options (char *line)
{
  while ( *line == '-' && line[1] == '-' )
    {
      while (*line && !spacep (line))
        line++;
      while (spacep (line))
        line++;
    }
  return line;
}


/* Common code for get_cert_local and get_issuer_cert_local. */
static ksba_cert_t 
do_get_cert_local (ctrl_t ctrl, const char *name, const char *command)
{
  unsigned char *value;
  size_t valuelen; 
  int rc;
  char *buf;
  ksba_cert_t cert;

  if (name)
    {
      buf = xmalloc ( strlen (command) + 1 + strlen(name) + 1);
      strcpy (stpcpy (stpcpy (buf, command), " "), name);
    }
  else
    buf = xstrdup (command);

  rc = assuan_inquire (ctrl->server_local->assuan_ctx, buf,
                       &value, &valuelen, MAX_CERT_LENGTH);
  xfree (buf);
  if (rc)
    {
      log_error (_("assuan_inquire(%s) failed: %s\n"),
                 command, gpg_strerror (rc));
      return NULL;
    }
  
  if (!valuelen)
    {
      xfree (value);
      return NULL;
    }

  rc = ksba_cert_new (&cert);
  if (!rc)
    {
      rc = ksba_cert_init_from_mem (cert, value, valuelen);
      if (rc)
        {
          ksba_cert_release (cert);
          cert = NULL;
        }
    }
  xfree (value);
  return cert;
}



/* Ask back to return a certificate for name, given as a regular
   gpgsm certificate indentificates (e.g. fingerprint or one of the
   other methods).  Alternatively, NULL may be used for NAME to
   return the current target certificate. Either return the certificate
   in a KSBA object or NULL if it is not available.
*/
ksba_cert_t 
get_cert_local (ctrl_t ctrl, const char *name)
{
  if (!ctrl || !ctrl->server_local || !ctrl->server_local->assuan_ctx)
    {
      if (opt.debug)
        log_debug ("get_cert_local called w/o context\n");
      return NULL;
    }
  return do_get_cert_local (ctrl, name, "SENDCERT");

}
       
/* Ask back to return the issuing certificate for name, given as a
   regular gpgsm certificate indentificates (e.g. fingerprint or one
   of the other methods).  Alternatively, NULL may be used for NAME to
   return thecurrent target certificate. Either return the certificate
   in a KSBA object or NULL if it is not available.
   
*/
ksba_cert_t 
get_issuing_cert_local (ctrl_t ctrl, const char *name)
{
  if (!ctrl || !ctrl->server_local || !ctrl->server_local->assuan_ctx)
    {
      if (opt.debug)
        log_debug ("get_issuing_cert_local called w/o context\n");
      return NULL;
    }
  return do_get_cert_local (ctrl, name, "SENDISSUERCERT");
}

/* Ask back to return a certificate with subject NAME and a
   subjectKeyIdentifier of KEYID. */
ksba_cert_t 
get_cert_local_ski (ctrl_t ctrl, const char *name, ksba_sexp_t keyid)
{
  unsigned char *value;
  size_t valuelen; 
  int rc;
  char *buf;
  ksba_cert_t cert;
  char *hexkeyid;

  if (!ctrl || !ctrl->server_local || !ctrl->server_local->assuan_ctx)
    {
      if (opt.debug)
        log_debug ("get_cert_local_ski called w/o context\n");
      return NULL;
    }
  if (!name || !keyid)
    {
      log_debug ("get_cert_local_ski called with insufficient arguments\n");
      return NULL;
    }

  hexkeyid = serial_hex (keyid);
  if (!hexkeyid)
    {
      log_debug ("serial_hex() failed\n");
      return NULL;
    }

  buf = xtrymalloc (15 + strlen (hexkeyid) + 2 + strlen(name) + 1);
  if (!buf)
    {

      log_error ("can't allocate enough memory: %s\n", strerror (errno));
      xfree (hexkeyid);
      return NULL;
    }
  strcpy (stpcpy (stpcpy (stpcpy (buf, "SENDCERT_SKI "), hexkeyid)," /"),name);
  xfree (hexkeyid);

  rc = assuan_inquire (ctrl->server_local->assuan_ctx, buf,
                       &value, &valuelen, MAX_CERT_LENGTH);
  xfree (buf);
  if (rc)
    {
      log_error (_("assuan_inquire(%s) failed: %s\n"), "SENDCERT_SKI",
                 gpg_strerror (rc));
      return NULL;
    }
  
  if (!valuelen)
    {
      xfree (value);
      return NULL;
    }

  rc = ksba_cert_new (&cert);
  if (!rc)
    {
      rc = ksba_cert_init_from_mem (cert, value, valuelen);
      if (rc)
        {
          ksba_cert_release (cert);
          cert = NULL;
        }
    }
  xfree (value);
  return cert;
}


/* Ask the client via an inquiry to check the istrusted status of the
   certificate specified by the hexified fingerprint HEXFPR.  Returns
   0 if the certificate is trusted by the client or an error code.  */
gpg_error_t
get_istrusted_from_client (ctrl_t ctrl, const char *hexfpr)
{
  unsigned char *value;
  size_t valuelen; 
  int rc;
  char request[100];

  if (!ctrl || !ctrl->server_local || !ctrl->server_local->assuan_ctx
      || !hexfpr)
    return gpg_error (GPG_ERR_INV_ARG);
  
  snprintf (request, sizeof request, "ISTRUSTED %s", hexfpr);
  rc = assuan_inquire (ctrl->server_local->assuan_ctx, request,
                       &value, &valuelen, 100);
  if (rc)
    {
      log_error (_("assuan_inquire(%s) failed: %s\n"),
                 request, gpg_strerror (rc));
      return rc;
    }
  /* The expected data is: "1" or "1 cruft" (not a C-string).  */
  if (valuelen && *value == '1' && (valuelen == 1 || spacep (value+1)))
    rc = 0;
  else
    rc = gpg_error (GPG_ERR_NOT_TRUSTED);
  xfree (value);
  return rc;
}




/* Ask the client to return the certificate associated with the
   current command. This is sometimes needed because the client usually
   sends us just the cert ID, assuming that the request can be
   satisfied from the cache, where the cert ID is used as key. */
static int
inquire_cert_and_load_crl (assuan_context_t ctx)
{
  ctrl_t ctrl = assuan_get_pointer (ctx);
  gpg_error_t err;
  unsigned char *value = NULL;
  size_t valuelen; 
  ksba_cert_t cert = NULL;

  err = assuan_inquire( ctx, "SENDCERT", &value, &valuelen, 0);
  if (err)
    return err;

/*   { */
/*     FILE *fp = fopen ("foo.der", "r"); */
/*     value = xmalloc (2000); */
/*     valuelen = fread (value, 1, 2000, fp); */
/*     fclose (fp); */
/*   } */

  if (!valuelen) /* No data returned; return a comprehensible error. */
    return gpg_error (GPG_ERR_MISSING_CERT);

  err = ksba_cert_new (&cert);
  if (err)
    goto leave;
  err = ksba_cert_init_from_mem (cert, value, valuelen);
  if(err)
    goto leave;
  xfree (value); value = NULL;

  err = crl_cache_reload_crl (ctrl, cert);

 leave:
  ksba_cert_release (cert);
  xfree (value);
  return err;
}


/* Handle OPTION commands. */
static int
option_handler (assuan_context_t ctx, const char *key, const char *value)
{
  ctrl_t ctrl = assuan_get_pointer (ctx);

  if (!strcmp (key, "force-crl-refresh"))
    {
      int i = *value? atoi (value) : 0;
      ctrl->force_crl_refresh = i;
    }
  else if (!strcmp (key, "audit-events"))
    {
      int i = *value? atoi (value) : 0;
      ctrl->audit_events = i;
    }
  else
    return gpg_error (GPG_ERR_UNKNOWN_OPTION);

  return 0;
}


static int
cmd_ldapserver (assuan_context_t ctx, char *line)
{
  ctrl_t ctrl = assuan_get_pointer (ctx);
  ldap_server_t server;
  ldap_server_t *last_next_p;

  while (spacep (line))
    line++;
  if (*line == '\0')
    return PARM_ERROR (_("ldapserver missing"));

  server = ldapserver_parse_one (line, "", 0);
  if (! server)
    return gpg_error (GPG_ERR_INV_ARG);

  last_next_p = &ctrl->server_local->ldapservers;
  while (*last_next_p)
    last_next_p = &(*last_next_p)->next;
  *last_next_p = server;
  return 0;
}


/* ISVALID [--only-ocsp] [--force-default-responder]
            <certificate_id>|<certificate_fpr>
  
   This command checks whether the certificate identified by the
   certificate_id is valid.  This is done by consulting CRLs or
   whatever has been configured.  Note, that the returned error codes
   are from gpg-error.h.  The command may callback using the inquire
   function.  See the manual for details.
 
   The CERTIFICATE_ID is a hex encoded string consisting of two parts,
   delimited by a single dot.  The first part is the SHA-1 hash of the
   issuer name and the second part the serial number.

   Alternatively the certificate's fingerprint may be given in which
   case an OCSP request is done before consulting the CRL.

   If the option --only-ocsp is given, no fallback to a CRL check will
   be used.

   If the option --force-default-responder is given, only the default
   OCSP responder will be used and any other methods of obtaining an
   OCSP responder URL won't be used.
 */

static int
cmd_isvalid (assuan_context_t ctx, char *line)
{
  ctrl_t ctrl = assuan_get_pointer (ctx);
  char *issuerhash, *serialno;
  gpg_error_t err;
  int did_inquire = 0;
  int ocsp_mode = 0;
  int only_ocsp;
  int force_default_responder;
  
  only_ocsp = has_option (line, "--only-ocsp");
  force_default_responder = has_option (line, "--force-default-responder");
  line = skip_options (line);

  issuerhash = xstrdup (line); /* We need to work on a copy of the
                                  line because that same Assuan
                                  context may be used for an inquiry.
                                  That is because Assuan reuses its
                                  line buffer.
                                   */

  serialno = strchr (issuerhash, '.');
  if (serialno)
    *serialno++ = 0;
  else
    {
      char *endp = strchr (issuerhash, ' ');
      if (endp)
        *endp = 0;
      if (strlen (issuerhash) != 40)
        {
          xfree (issuerhash);
          return PARM_ERROR (_("serialno missing in cert ID"));
        }
      ocsp_mode = 1;
    }


 again:
  if (ocsp_mode)
    {
      /* Note, that we ignore the given issuer hash and instead rely
         on the current certificate semantics used with this
         command. */
      if (!opt.allow_ocsp)
        err = gpg_error (GPG_ERR_NOT_SUPPORTED);
      else
        err = ocsp_isvalid (ctrl, NULL, NULL, force_default_responder);
      /* Fixme: If we got no ocsp response and --only-ocsp is not used
         we should fall back to CRL mode.  Thus we need to clear
         OCSP_MODE, get the issuerhash and the serialno from the
         current certificate and jump to again. */
    }
  else if (only_ocsp)
    err = gpg_error (GPG_ERR_NO_CRL_KNOWN);
  else 
    {
      switch (crl_cache_isvalid (ctrl,
                                 issuerhash, serialno,
                                 ctrl->force_crl_refresh))
        {
        case CRL_CACHE_VALID:
          err = 0;
          break;
        case CRL_CACHE_INVALID:
          err = gpg_error (GPG_ERR_CERT_REVOKED);
          break;
        case CRL_CACHE_DONTKNOW: 
          if (did_inquire)
            err = gpg_error (GPG_ERR_NO_CRL_KNOWN);
          else if (!(err = inquire_cert_and_load_crl (ctx)))
            {
              did_inquire = 1;
              goto again;
            }
          break;
        case CRL_CACHE_CANTUSE: 
          err = gpg_error (GPG_ERR_NO_CRL_KNOWN);
          break;
        default:
          log_fatal ("crl_cache_isvalid returned invalid code\n");
        }
    }

  if (err)
    log_error (_("command %s failed: %s\n"), "ISVALID", gpg_strerror (err));
  xfree (issuerhash);
  return err;
}


/* If the line contains a SHA-1 fingerprint as the first argument,
   return the FPR vuffer on success.  The function checks that the
   fingerprint consists of valid characters and prints and error
   message if it does not and returns NULL.  Fingerprints are
   considered optional and thus no explicit error is returned. NULL is
   also returned if there is no fingerprint at all available. 
   FPR must be a caller provided buffer of at least 20 bytes.

   Note that colons within the fingerprint are allowed to separate 2
   hex digits; this allows for easier cutting and pasting using the
   usual fingerprint rendering.
*/
static unsigned char *
get_fingerprint_from_line (const char *line, unsigned char *fpr)
{
  const char *s;
  int i;

  for (s=line, i=0; *s && *s != ' '; s++ )
    {
      if ( hexdigitp (s) && hexdigitp (s+1) )
        {
          if ( i >= 20 )
            return NULL;  /* Fingerprint too long.  */
          fpr[i++] = xtoi_2 (s);
          s++;
        }
      else if ( *s != ':' )
        return NULL; /* Invalid.  */
    }
  if ( i != 20 )
    return NULL; /* Fingerprint to short.  */
  return fpr;
}



/* CHECKCRL [<fingerprint>]

   Check whether the certificate with FINGERPRINT (SHA-1 hash of the
   entire X.509 certificate blob) is valid or not by consulting the
   CRL responsible for this certificate.  If the fingerprint has not
   been given or the certificate is not known, the function 
   inquires the certificate using an

      INQUIRE TARGETCERT

   and the caller is expected to return the certificate for the
   request (which should match FINGERPRINT) as a binary blob.
   Processing then takes place without further interaction; in
   particular dirmngr tries to locate other required certificate by
   its own mechanism which includes a local certificate store as well
   as a list of trusted root certificates.

   The return value is the usual gpg-error code or 0 for ducesss;
   i.e. the certificate validity has been confirmed by a valid CRL.
*/
static int
cmd_checkcrl (assuan_context_t ctx, char *line)
{
  ctrl_t ctrl = assuan_get_pointer (ctx);
  gpg_error_t err;
  unsigned char fprbuffer[20], *fpr;
  ksba_cert_t cert;

  fpr = get_fingerprint_from_line (line, fprbuffer);
  cert = fpr? get_cert_byfpr (fpr) : NULL;
  
  if (!cert)
    {
      /* We do not have this certificate yet or the fingerprint has
         not been given.  Inquire it from the client.  */
      unsigned char *value = NULL;
      size_t valuelen; 
      
      err = assuan_inquire (ctrl->server_local->assuan_ctx, "TARGETCERT",
                           &value, &valuelen, MAX_CERT_LENGTH);
      if (err)
        {
          log_error (_("assuan_inquire failed: %s\n"), gpg_strerror (err));
          goto leave;
        }
  
      if (!valuelen) /* No data returned; return a comprehensible error. */
        err = gpg_error (GPG_ERR_MISSING_CERT);
      else
        {
          err = ksba_cert_new (&cert);
          if (!err)
            err = ksba_cert_init_from_mem (cert, value, valuelen);
        }
      xfree (value);
      if(err)
        goto leave;
    }

  assert (cert);

  err = crl_cache_cert_isvalid (ctrl, cert, ctrl->force_crl_refresh);
  if (gpg_err_code (err) == GPG_ERR_NO_CRL_KNOWN)
    {
      err = crl_cache_reload_crl (ctrl, cert);
      if (!err)
        err = crl_cache_cert_isvalid (ctrl, cert, 0);
    }

 leave:
  if (err)
    log_error (_("command %s failed: %s\n"), "CHECKCRL", gpg_strerror (err));
  ksba_cert_release (cert);
  return err;
}


/* CHECKOCSP [--force-default-responder] [<fingerprint>]

   Check whether the certificate with FINGERPRINT (SHA-1 hash of the
   entire X.509 certificate blob) is valid or not by asking an OCSP
   responder responsible for this certificate.  The optional
   fingerprint may be used for a quick check in case an OCSP check has
   been done for this certificate recently (we always cache OCSP
   responses for a couple of minutes). If the fingerprint has not been
   given or there is no cached result, the function inquires the
   certificate using an

      INQUIRE TARGETCERT

   and the caller is expected to return the certificate for the
   request (which should match FINGERPRINT) as a binary blob.
   Processing then takes place without further interaction; in
   particular dirmngr tries to locate other required certificates by
   its own mechanism which includes a local certificate store as well
   as a list of trusted root certifciates.

   If the option --force-default-responder is given, only the default
   OCSP responder will be used and any other methods of obtaining an
   OCSP responder URL won't be used.

   The return value is the usual gpg-error code or 0 for ducesss;
   i.e. the certificate validity has been confirmed by a valid CRL.
*/
static int
cmd_checkocsp (assuan_context_t ctx, char *line)
{
  ctrl_t ctrl = assuan_get_pointer (ctx);
  gpg_error_t err;
  unsigned char fprbuffer[20], *fpr;
  ksba_cert_t cert;
  int force_default_responder;
  
  force_default_responder = has_option (line, "--force-default-responder");
  line = skip_options (line);

  fpr = get_fingerprint_from_line (line, fprbuffer);
  cert = fpr? get_cert_byfpr (fpr) : NULL;
  
  if (!cert)
    {
      /* We do not have this certificate yet or the fingerprint has
         not been given.  Inquire it from the client.  */
      unsigned char *value = NULL;
      size_t valuelen; 
      
      err = assuan_inquire (ctrl->server_local->assuan_ctx, "TARGETCERT",
                           &value, &valuelen, MAX_CERT_LENGTH);
      if (err)
        {
          log_error (_("assuan_inquire failed: %s\n"), gpg_strerror (err));
          goto leave;
        }
  
      if (!valuelen) /* No data returned; return a comprehensible error. */
        err = gpg_error (GPG_ERR_MISSING_CERT);
      else
        {
          err = ksba_cert_new (&cert);
          if (!err)
            err = ksba_cert_init_from_mem (cert, value, valuelen);
        }
      xfree (value);
      if(err)
        goto leave;
    }

  assert (cert);

  if (!opt.allow_ocsp)
    err = gpg_error (GPG_ERR_NOT_SUPPORTED);
  else
    err = ocsp_isvalid (ctrl, cert, NULL, force_default_responder);

 leave:
  if (err)
    log_error (_("command %s failed: %s\n"), "CHECKOCSP", gpg_strerror (err));
  ksba_cert_release (cert);
  return err;
}



static int
lookup_cert_by_url (assuan_context_t ctx, const char *url)
{
  ctrl_t ctrl = assuan_get_pointer (ctx);
  gpg_error_t err = 0;
  unsigned char *value = NULL;
  size_t valuelen; 

  /* Fetch single certificate given it's URL.  */
  err = fetch_cert_by_url (ctrl, url, &value, &valuelen);
  if (err)
    {
      log_error (_("fetch_cert_by_url failed: %s\n"), gpg_strerror (err));
      goto leave;
    }

  /* Send the data, flush the buffer and then send an END. */
  err = assuan_send_data (ctx, value, valuelen);      
  if (!err)
    err = assuan_send_data (ctx, NULL, 0);
  if (!err)
    err = assuan_write_line (ctx, "END");
  if (err) 
    {
      log_error (_("error sending data: %s\n"), gpg_strerror (err));
      goto leave;
    }

 leave:

  return err;
}


/* Send the certificate, flush the buffer and then send an END. */
static gpg_error_t
return_one_cert (void *opaque, ksba_cert_t cert)
{
  assuan_context_t ctx = opaque;
  gpg_error_t err;
  const unsigned char *der;
  size_t derlen;

  der = ksba_cert_get_image (cert, &derlen);
  if (!der)
    err = gpg_error (GPG_ERR_INV_CERT_OBJ);
  else
    {
      err = assuan_send_data (ctx, der, derlen);      
      if (!err)
        err = assuan_send_data (ctx, NULL, 0);
      if (!err)
        err = assuan_write_line (ctx, "END");
    }
  if (err) 
    log_error (_("error sending data: %s\n"), gpg_strerror (err));
  return err;
}


/* Lookup certificates from the internal cache or using the ldap
   servers. */
static int
lookup_cert_by_pattern (assuan_context_t ctx, char *line, 
                        int single, int cache_only)
{
  ctrl_t ctrl = assuan_get_pointer (ctx);
  gpg_error_t err = 0;
  char *p;
  strlist_t sl, list = NULL;
  int truncated = 0, truncation_forced = 0;
  int count = 0;
  int local_count = 0;
  unsigned char *value = NULL;
  size_t valuelen; 
  struct ldapserver_iter ldapserver_iter;
  cert_fetch_context_t fetch_context;
  int any_no_data = 0;

  /* Break the line down into an STRLIST */
  for (p=line; *p; line = p)
    {
      while (*p && *p != ' ')
        p++;
      if (*p) 
        *p++ = 0;

      if (*line)
        {
          sl = xtrymalloc (sizeof *sl + strlen (line));
          if (!sl)
            {
              err = gpg_error_from_errno (errno);
              goto leave;
            }
          memset (sl, 0, sizeof *sl);
          strcpy_escaped_plus (sl->d, line);
          sl->next = list;
          list = sl;
        }
    }

  /* First look through the internal cache.  The certifcates retruned
     here are not counted towards the truncation limit.  */
  if (single && !cache_only)
    ; /* Do not read from the local cache in this case.  */
  else
    {
      for (sl=list; sl; sl = sl->next)
        {
          err = get_certs_bypattern (sl->d, return_one_cert, ctx);
          if (!err)
            local_count++;
          if (!err && single)
            goto ready; 

          if (gpg_err_code (err) == GPG_ERR_NO_DATA)
            {
              err = 0;
              if (cache_only)
                any_no_data = 1;
            }
          else if (gpg_err_code (err) == GPG_ERR_INV_NAME && !cache_only)
            {
              /* No real fault because the internal pattern lookup
                 can't yet cope with all types of pattern.  */
              err = 0;
            }
          if (err)
            goto ready;
        }
    }

  /* Loop over all configured servers unless we want only the
     certificates from the cache.  */
  for (ldapserver_iter_begin (&ldapserver_iter, ctrl);
       !cache_only && !ldapserver_iter_end_p (&ldapserver_iter)
	 && ldapserver_iter.server->host && !truncation_forced;
       ldapserver_iter_next (&ldapserver_iter))
    {
      ldap_server_t ldapserver = ldapserver_iter.server;
      
      if (DBG_LOOKUP)
        log_debug ("cmd_lookup: trying %s:%d base=%s\n", 
                   ldapserver->host, ldapserver->port,
                   ldapserver->base?ldapserver->base : "[default]");

      /* Fetch certificates matching pattern */
      err = start_cert_fetch (ctrl, &fetch_context, list, ldapserver);
      if ( gpg_err_code (err) == GPG_ERR_NO_DATA )
        {
          if (DBG_LOOKUP)
            log_debug ("cmd_lookup: no data\n");
          err = 0;
          any_no_data = 1;
          continue;
        }
      if (err)
        {
          log_error (_("start_cert_fetch failed: %s\n"), gpg_strerror (err));
          goto leave;
        }

      /* Fetch the certificates for this query. */
      while (!truncation_forced)
        {
          xfree (value); value = NULL;
          err = fetch_next_cert (fetch_context, &value, &valuelen);
          if (gpg_err_code (err) == GPG_ERR_NO_DATA )
            {
              err = 0;
              any_no_data = 1;
              break; /* Ready. */
            }
          if (gpg_err_code (err) == GPG_ERR_TRUNCATED)
            {
              truncated = 1;
              err = 0;
              break;  /* Ready.  */
            }
          if (gpg_err_code (err) == GPG_ERR_EOF)
            {
              err = 0;
              break; /* Ready. */
            }
          if (!err && !value)
            {
              err = gpg_error (GPG_ERR_BUG);
              goto leave;
            }
          if (err)
            {
              log_error (_("fetch_next_cert failed: %s\n"),
                         gpg_strerror (err));
              end_cert_fetch (fetch_context);
              goto leave;
            }
          
          if (DBG_LOOKUP)
            log_debug ("cmd_lookup: returning one cert%s\n",
                       truncated? " (truncated)":"");
          
          /* Send the data, flush the buffer and then send an END line
             as a certificate delimiter. */
          err = assuan_send_data (ctx, value, valuelen);      
          if (!err)
            err = assuan_send_data (ctx, NULL, 0);
          if (!err)
            err = assuan_write_line (ctx, "END");
          if (err) 
            {
              log_error (_("error sending data: %s\n"), gpg_strerror (err));
              end_cert_fetch (fetch_context);
              goto leave;
            }
          
          if (++count >= opt.max_replies )
            {
              truncation_forced = 1;
              log_info (_("max_replies %d exceeded\n"), opt.max_replies );
            }
          if (single)
            break;
        }

      end_cert_fetch (fetch_context);
    }

 ready:
  if (truncated || truncation_forced)
    {
      char str[50];

      sprintf (str, "%d", count);
      assuan_write_status (ctx, "TRUNCATED", str);    
    }

  if (!err && !count && !local_count && any_no_data)
    err = gpg_error (GPG_ERR_NO_DATA);

 leave:
  free_strlist (list);
  return err;
}


/* LOOKUP [--url] [--single] [--cache-only] <pattern>

   Lookup certificates matching PATTERN. With --url the pattern is
   expected to be one URL.

   If --url is not given:  To allow for multiple
   patterns (which are ORed) quoting is required: Spaces are to be
   translated into "+" or into "%20"; obviously this requires that the
   usual escape quoting rules are applied.

   If --url is given no special escaping is required because URLs are
   already escaped this way.

   if --single is given the first and only the first match will be
   returned.  If --cache-only is _not_ given, no local query will be
   done.

   if --cache-only is given no external lookup is done so that only
   certificates from the cache may get returned.
*/

static int
cmd_lookup (assuan_context_t ctx, char *line)
{
  gpg_error_t err;
  int lookup_url, single, cache_only;

  lookup_url = has_leading_option (line, "--url");
  single = has_leading_option (line, "--single");
  cache_only = has_leading_option (line, "--cache-only");
  line = skip_options (line);

  if (lookup_url && cache_only)
    err = gpg_error (GPG_ERR_NOT_FOUND);
  else if (lookup_url && single)
    err = gpg_error (GPG_ERR_NOT_IMPLEMENTED);
  else if (lookup_url)
    err = lookup_cert_by_url (ctx, line);
  else
    err = lookup_cert_by_pattern (ctx, line, single, cache_only);

  if (err)
    log_error (_("command %s failed: %s\n"), "LOOKUP", gpg_strerror (err));

  return err;
}


/* LOADCRL <filename>

   Load the CRL in the file with name FILENAME into our cache.  Note
   that FILENAME should be given with an absolute path because
   Dirmngrs cwd is not known.  This command is usually used by gpgsm
   using the invocation "gpgsm --call-dirmngr loadcrl <filename>".  A
   direct invocation of Dirmngr is not useful because gpgsm might need
   to callback gpgsm to ask for the CA's certificate.
*/

static int
cmd_loadcrl (assuan_context_t ctx, char *line)
{
  ctrl_t ctrl = assuan_get_pointer (ctx);
  gpg_error_t err;
  char *buf;

  buf = xmalloc (strlen (line)+1);
  strcpy_escaped_plus (buf, line);
  err = crl_cache_load (ctrl, buf);
  xfree (buf);
  if (err)
    log_error (_("command %s failed: %s\n"), "LOADCRL", gpg_strerror (err));
  return err;
}


/* LISTCRLS 

   List the content of all CRLs in a readable format.  This command is
   usually used by gpgsm using the invocation "gpgsm --call-dirmngr
   listcrls".  It may also be used directly using "dirmngr
   --list-crls".
*/

static int
cmd_listcrls (assuan_context_t ctx, char *line)
{
  gpg_error_t err;
  FILE *fp = assuan_get_data_fp (ctx);

  (void)line;

  if (!fp)
    return PARM_ERROR (_("no data stream"));

  err = crl_cache_list (fp);
  if (err)
    log_error (_("command %s failed: %s\n"), "LISTCRLS", gpg_strerror (err));
  return err;
}


/* CACHECERT 

   Put a certificate into the internal cache.  This command might be
   useful if a client knows in advance certificates required for a
   test and wnats to make sure they get added to the internal cache.
   It is also helpful for debugging.  To get the actual certificate,
   this command immediately inquires it using

      INQUIRE TARGETCERT

   and the caller is expected to return the certificate for the
   request as a binary blob.
*/
static int
cmd_cachecert (assuan_context_t ctx, char *line)
{
  ctrl_t ctrl = assuan_get_pointer (ctx);
  gpg_error_t err;
  ksba_cert_t cert = NULL;
  unsigned char *value = NULL;
  size_t valuelen; 

  (void)line;
      
  err = assuan_inquire (ctrl->server_local->assuan_ctx, "TARGETCERT",
                       &value, &valuelen, MAX_CERT_LENGTH);
  if (err)
    {
      log_error (_("assuan_inquire failed: %s\n"), gpg_strerror (err));
      goto leave;
    }
  
  if (!valuelen) /* No data returned; return a comprehensible error. */
    err = gpg_error (GPG_ERR_MISSING_CERT);
  else
    {
      err = ksba_cert_new (&cert);
      if (!err)
        err = ksba_cert_init_from_mem (cert, value, valuelen);
    }
  xfree (value);
  if(err)
    goto leave;

  err = cache_cert (cert);

 leave:
  if (err)
    log_error (_("command %s failed: %s\n"), "CACHECERT", gpg_strerror (err));
  ksba_cert_release (cert);
  return err;
}


/* VALIDATE 

   Validate a certificate using the certificate validation function
   used internally by dirmngr.  This command is only useful for
   debugging.  To get the actual certificate, this command immediately
   inquires it using

      INQUIRE TARGETCERT

   and the caller is expected to return the certificate for the
   request as a binary blob.
*/
static int
cmd_validate (assuan_context_t ctx, char *line)
{
  ctrl_t ctrl = assuan_get_pointer (ctx);
  gpg_error_t err;
  ksba_cert_t cert = NULL;
  unsigned char *value = NULL;
  size_t valuelen; 

  (void)line;
    
  err = assuan_inquire (ctrl->server_local->assuan_ctx, "TARGETCERT",
                       &value, &valuelen, MAX_CERT_LENGTH);
  if (err)
    {
      log_error (_("assuan_inquire failed: %s\n"), gpg_strerror (err));
      goto leave;
    }
  
  if (!valuelen) /* No data returned; return a comprehensible error. */
    err = gpg_error (GPG_ERR_MISSING_CERT);
  else
    {
      err = ksba_cert_new (&cert);
      if (!err)
        err = ksba_cert_init_from_mem (cert, value, valuelen);
    }
  xfree (value);
  if(err)
    goto leave;

  /* If we have this certificate already in our cache, use the cached
     version for validation because this will take care of any cached
     results. */
  { 
    unsigned char fpr[20];
    ksba_cert_t tmpcert;

    cert_compute_fpr (cert, fpr);
    tmpcert = get_cert_byfpr (fpr);
    if (tmpcert)
      {
        ksba_cert_release (cert);
        cert = tmpcert;
      }
  }

  err = validate_cert_chain (ctrl, cert, NULL, VALIDATE_MODE_CERT, NULL);

 leave:
  if (err)
    log_error (_("command %s failed: %s\n"), "VALIDATE", gpg_strerror (err));
  ksba_cert_release (cert);
  return err;
}



/* Tell the assuan library about our commands. */
static int
register_commands (assuan_context_t ctx)
{
  static struct {
    const char *name;
    int (*handler)(assuan_context_t, char *line);
  } table[] = {
    { "LDAPSERVER", cmd_ldapserver },
    { "ISVALID",    cmd_isvalid },
    { "CHECKCRL",   cmd_checkcrl },
    { "CHECKOCSP",  cmd_checkocsp },
    { "LOOKUP",     cmd_lookup },
    { "LOADCRL",    cmd_loadcrl },
    { "LISTCRLS",   cmd_listcrls },
    { "CACHECERT",  cmd_cachecert },
    { "VALIDATE",   cmd_validate },
    { "INPUT",      NULL },
    { "OUTPUT",     NULL },
    { NULL, NULL }
  };
  int i, j, rc;

  for (i=j=0; table[i].name; i++)
    {
      rc = assuan_register_command (ctx, table[i].name, table[i].handler);
      if (rc)
        return rc;
    }
  return 0;
}


static void
reset_notify (assuan_context_t ctx)
{
  ctrl_t ctrl = assuan_get_pointer (ctx);

  ldapserver_list_free (ctrl->server_local->ldapservers);
  ctrl->server_local->ldapservers = NULL;
}


/* Startup the server and run the main command loop.  With FD = -1
   used stdin/stdout. */
void
start_command_handler (assuan_fd_t fd)
{
  static const char hello[] = "Dirmngr " VERSION " at your service";
  static char *hello_line;
  int rc;
  assuan_context_t ctx;
  ctrl_t ctrl;

  ctrl = xtrycalloc (1, sizeof *ctrl);
  if (ctrl)
    ctrl->server_local = xtrycalloc (1, sizeof *ctrl->server_local);
  if (!ctrl || !ctrl->server_local)
    {
      log_error (_("can't allocate control structure: %s\n"),
                 strerror (errno));
      xfree (ctrl);
      return;
    }
    
  dirmngr_init_default_ctrl (ctrl);

  if (fd == ASSUAN_INVALID_FD)
    {
      int filedes[2];

      filedes[0] = 0;
      filedes[1] = 1;
      rc = assuan_init_pipe_server (&ctx, filedes);
    }
  else
    {
      rc = assuan_init_socket_server_ext (&ctx, fd, 2);
    }

  if (rc)
    {
      log_error (_("failed to initialize the server: %s\n"),
                 gpg_strerror(rc));
      dirmngr_exit (2);
    }

  rc = register_commands (ctx);
  if (rc)
    {
      log_error (_("failed to the register commands with Assuan: %s\n"),
                 gpg_strerror(rc));
      dirmngr_exit (2);
    }


  if (!hello_line)
    {
      size_t n;
      const char *cfgname;

      cfgname = opt.config_filename? opt.config_filename : "[none]";

      n = (30 + strlen (opt.homedir) + strlen (cfgname)
           + strlen (hello) + 1);
      hello_line = xmalloc (n+1);
      snprintf (hello_line, n,
                "Home: %s\n"
                "Config: %s\n"
                "%s",
                opt.homedir,
                cfgname,
                hello);
      hello_line[n] = 0;
    }

  ctrl->server_local->assuan_ctx = ctx;
  assuan_set_pointer (ctx, ctrl);

  assuan_set_hello_line (ctx, hello_line);
  assuan_register_option_handler (ctx, option_handler);
  assuan_register_reset_notify (ctx, reset_notify);

  if (DBG_ASSUAN)
    assuan_set_log_stream (ctx, log_get_stream ());

  for (;;) 
    {
      rc = assuan_accept (ctx);
      if (rc == -1)
        break;
      if (rc)
        {
          log_info (_("Assuan accept problem: %s\n"), gpg_strerror (rc));
          break;
        }

#ifndef HAVE_W32_SYSTEM
      if (opt.verbose)
        {
          pid_t apid;
          uid_t auid;
          gid_t agid;

          if (!assuan_get_peercred (ctx, &apid, &auid, &agid))
            log_info ("connection from process %ld (%ld:%ld)\n",
                      (long)apid, (long)auid, (long)agid);
        }
#endif

      rc = assuan_process (ctx);
      if (rc)
        {
          log_info (_("Assuan processing failed: %s\n"), gpg_strerror (rc));
          continue;
        }
    }
  
  ldap_wrapper_connection_cleanup (ctrl);

  ldapserver_list_free (ctrl->server_local->ldapservers);
  ctrl->server_local->ldapservers = NULL;

  ctrl->server_local->assuan_ctx = NULL;
  assuan_deinit_server (ctx);

  if (ctrl->refcount)
    log_error ("oops: connection control structure still referenced (%d)\n",
               ctrl->refcount);
  else
    {
      release_ctrl_ocsp_certs (ctrl);
      xfree (ctrl->server_local);
      xfree (ctrl);
    }
}


/* Send a status line back to the client.  KEYWORD is the status
   keyword, the optioal string argumenst are blank separated added to
   the line, the last argument must be a NULL. */
gpg_error_t
dirmngr_status (ctrl_t ctrl, const char *keyword, ...)
{
  gpg_error_t err = 0;
  va_list arg_ptr;
  const char *text;

  va_start (arg_ptr, keyword);

  if (ctrl->server_local)
    {
      assuan_context_t ctx = ctrl->server_local->assuan_ctx;
      char buf[950], *p;
      size_t n;
      
      p = buf; 
      n = 0;
      while ( (text = va_arg (arg_ptr, const char *)) )
        {
          if (n)
            {
              *p++ = ' ';
              n++;
            }
          for ( ; *text && n < DIM (buf)-2; n++)
            *p++ = *text++;
        }
      *p = 0;
      err = assuan_write_status (ctx, keyword, buf);
    }

  va_end (arg_ptr);
  return err;
}


/* Note, that we ignore CTRL for now but use the first connection to
   send the progress info back. */
gpg_error_t
dirmngr_tick (ctrl_t ctrl)
{
  static time_t next_tick = 0;
  gpg_error_t err = 0;
  time_t now = time (NULL);

  if (!next_tick)
    {
      next_tick = now + 1;
    }
  else if ( now > next_tick )
    {
      if (ctrl)
        {
          err = dirmngr_status (ctrl, "PROGRESS", "tick", "? 0 0", NULL);
          if (err)
            {
              /* Take this as in indication for a cancel request.  */
              err = gpg_error (GPG_ERR_CANCELED);
            }
          now = time (NULL);
        }

      next_tick = now + 1;
    }
  return err;
}
