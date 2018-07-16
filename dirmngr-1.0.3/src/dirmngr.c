/* dirmngr.c - LDAP access
 *	Copyright (C) 2002 Klarälvdalens Datakonsult AB
 *      Copyright (C) 2003, 2004, 2006, 2007, 2008 g10 Code GmbH
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <fcntl.h>
#ifndef HAVE_W32_SYSTEM
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <pth.h>

#include <gcrypt.h>
#include <ksba.h>
#include <assuan.h> /* Needed for the malloc and socket hooks */

#define JNLIB_NEED_LOG_LOGV
#include "dirmngr.h"
#include "certcache.h"
#include "crlcache.h"
#include "crlfetch.h"
#include "misc.h"
#include "i18n.h"
#include "ldapserver.h"

#ifdef HAVE_W32_SYSTEM
#define sleep _sleep
#define FD2INT(h) ((unsigned int) (h))
#else
#define FD2INT(h) (h)
#endif

enum cmd_and_opt_values 
{ aNull = 0,
  oCsh		  = 'c',
  oQuiet	  = 'q',
  oSh		  = 's',
  oVerbose	  = 'v',
  oNoVerbose = 500,
  
  aServer,
  aDaemon,
  aService,
  aListCRLs,
  aLoadCRL,
  aFetchCRL,
  aShutdown,
  aFlush,
  aGPGConfList,
  aGPGConfTest,

  oOptions,
  oDebug,
  oDebugAll,
  oDebugWait,
  oDebugLevel,
  oNoGreeting,
  oNoOptions,
  oHomedir,
  oNoDetach,
  oLogFile,
  oBatch,
  oDisableHTTP,
  oDisableLDAP,
  oIgnoreLDAPDP,
  oIgnoreHTTPDP,
  oIgnoreOCSPSvcUrl,
  oHonorHTTPProxy,
  oHTTPProxy,
  oLDAPProxy,
  oOnlyLDAPProxy,
  oLDAPFile,
  oLDAPTimeout,
  oLDAPAddServers,
  oOCSPResponder,
  oOCSPSigner,
  oOCSPMaxClockSkew,
  oOCSPMaxPeriod,
  oOCSPCurrentPeriod,
  oMaxReplies,
  oFakedSystemTime,
  oForce,
  oAllowOCSP,
  oSocketName,
  oLDAPWrapperProgram,
  oHTTPWrapperProgram,
aTest };



static ARGPARSE_OPTS opts[] = {
  
  { 300, NULL, 0, N_("@Commands:\n ") },

  { aServer,   "server",    256, N_("run in server mode (foreground)") },
  { aDaemon,   "daemon",    256, N_("run in daemon mode (background)") },
#ifdef HAVE_W32_SYSTEM
  { aService,  "service",   256, N_("run as windows service (background)") },
#endif
  { aListCRLs, "list-crls", 256, N_("list the contents of the CRL cache")},
  { aLoadCRL,  "load-crl",  256, N_("|FILE|load CRL from FILE into cache")},
  { aFetchCRL, "fetch-crl", 256, N_("|URL|fetch a CRL from URL")},
  { aShutdown, "shutdown",  256, N_("shutdown the dirmngr")},
  { aFlush,    "flush",     256, N_("flush the cache")},
  { aGPGConfList, "gpgconf-list", 256, "@" },
  { aGPGConfTest, "gpgconf-test", 256, "@" },

  { 301, NULL, 0, N_("@\nOptions:\n ") },

  { oVerbose,  "verbose",   0, N_("verbose") },
  { oQuiet,    "quiet",     0, N_("be somewhat more quiet") },
  { oSh,       "sh",        0, N_("sh-style command output") },
  { oCsh,      "csh",       0, N_("csh-style command output") },
  { oOptions,  "options"  , 2, N_("|FILE|read options from FILE")},
  { oDebugLevel, "debug-level",2,
                               N_("|LEVEL|set the debugging level to LEVEL")},
  { oNoDetach, "no-detach" ,0, N_("do not detach from the console")},
  { oLogFile,  "log-file"  ,2, N_("|FILE|write server mode logs to FILE")},
  { oBatch   , "batch"     ,0, N_("run without asking a user")},
  { oForce,    "force"     ,0, N_("force loading of outdated CRLs")},
  { oAllowOCSP, "allow-ocsp",0,N_("allow sending OCSP requests")},
  { oDisableHTTP, "disable-http", 0, N_("inhibit the use of HTTP")},
  { oDisableLDAP, "disable-ldap", 0, N_("inhibit the use of LDAP")},
  { oIgnoreHTTPDP,"ignore-http-dp", 0,
    N_("ignore HTTP CRL distribution points")},
  { oIgnoreLDAPDP,"ignore-ldap-dp", 0,
    N_("ignore LDAP CRL distribution points")},
  { oIgnoreOCSPSvcUrl, "ignore-ocsp-service-url", 0,
    N_("ignore certificate contained OCSP service URLs")},
      /* Note: The next one is to fix a typo in gpgconf - should be
         removed eventually. */
  { oIgnoreOCSPSvcUrl, "ignore-ocsp-servic-url", 0, "@"},

  { oHTTPProxy,  "http-proxy", 2,
    N_("|URL|redirect all HTTP requests to URL")},
  { oLDAPProxy,  "ldap-proxy", 2,
    N_("|HOST|use HOST for LDAP queries")},
  { oOnlyLDAPProxy, "only-ldap-proxy", 0, 
    N_("do not use fallback hosts with --ldap-proxy")},

  { oLDAPFile, "ldapserverlist-file", 2,
    N_("|FILE|read LDAP server list from FILE")},
  { oLDAPAddServers, "add-servers", 0,
    N_("add new servers discovered in CRL distribution points to serverlist")},
  { oLDAPTimeout, "ldaptimeout", 1,
    N_("|N|set LDAP timeout to N seconds")},

  { oOCSPResponder, "ocsp-responder", 2, N_("|URL|use OCSP responder at URL")},
  { oOCSPSigner, "ocsp-signer", 2, N_("|FPR|OCSP response signed by FPR")}, 
  { oOCSPMaxClockSkew, "ocsp-max-clock-skew", 1, "@" },
  { oOCSPMaxPeriod, "ocsp-max-period", 1, "@" },
  { oOCSPCurrentPeriod, "ocsp-current-period", 1, "@" },

  { oMaxReplies, "max-replies", 1,
    N_("|N|do not return more than N items in one query")},

  { oSocketName, "socket-name", 2, N_("|FILE|listen on socket FILE") },

  { oFakedSystemTime, "faked-system-time", 4, "@" }, /* (epoch time) */
  { oDebug,    "debug"     ,4|16, "@"},
  { oDebugAll, "debug-all" ,0,    "@"},
  { oDebugWait, "debug-wait", 1, "@"},
  { oNoGreeting, "no-greeting", 0, "@"},
  { oHomedir, "homedir", 2, "@" },  
  { oLDAPWrapperProgram, "ldap-wrapper-program", 2, "@"},
  { oHTTPWrapperProgram, "http-wrapper-program", 2, "@"},
  { oHonorHTTPProxy,     "honor-http-proxy", 0, "@" },

  { 302, NULL, 0, N_(
  "@\n(See the \"info\" manual for a complete listing of all commands and options)\n"
                    )},

  { 0, NULL, 0, NULL }
};

#define DEFAULT_MAX_REPLIES 10
#define DEFAULT_LDAP_TIMEOUT 100 /* arbitrary large timeout */

/* For the cleanup handler we need to keep track of the socket's name. */
static const char *socket_name;

/* We need to keep track of the server's nonces (these are dummies for
   POSIX systems). */
static assuan_sock_nonce_t socket_nonce;

/* Only if this flag has been set we will remove the socket file.  */
static int cleanup_socket;

/* Keep track of the current log file so that we can avoid updating
   the log file after a SIGHUP if it didn't changed. Malloced. */
static char *current_logfile;

/* Helper to implement --debug-level. */
static const char *debug_level;

/* Flag indicating that a shutdown has been requested.  */
static volatile int shutdown_pending;

/* Counter for the active connections.  */
static int active_connections;

/* The timer tick used for housekeeping stuff.  For Windows we use a
   longer period as the SetWaitableTimer seems to signal earlier than
   the 2 seconds.  */
#ifdef HAVE_W32_SYSTEM
#define TIMERTICK_INTERVAL    (4)
#else
#define TIMERTICK_INTERVAL    (2)    /* Seconds.  */
#endif


/* Prototypes. */
static void cleanup (void);
static ldap_server_t parse_ldapserver_file (const char* filename);
static fingerprint_list_t parse_ocsp_signer (const char *string);
static void handle_connections (assuan_fd_t listen_fd);

/* Pth wrapper function definitions. */
GCRY_THREAD_OPTION_PTH_IMPL;


static const char *
my_strusage( int level )
{
  const char *p;
  switch ( level ) 
    {
    case 11: p = "dirmngr";
      break;
    case 13: p = VERSION; break;
    case 14: p = "Copyright (C) " COPYRIGHT_YEAR_NAME; break;
    case 17: p = PRINTABLE_OS_NAME; break;
      /* TRANSLATORS: @EMAIL@ will get replaced by the actual bug
         reporting address.  This is so that we can change the
         reporting address without breaking the translations.  */
    case 19: p = _("Please report bugs to <@EMAIL@>.\n"); break;
    case 49: p = PACKAGE_BUGREPORT; break;
    case 1:
    case 40: p = _("Usage: dirmngr [options] (-h for help)");
      break;
    case 41: p = _("Syntax: dirmngr [options] [command [args]]\n"
                   "LDAP and OCSP access for GnuPG\n");
      break;
      
    default: p = NULL;
    }
  return p;
}


/* Used by gcry for logging. */
static void
my_gcry_logger (void *dummy, int level, const char *fmt, va_list arg_ptr)
{
  (void)dummy;

  /* Translate the log levels. */
  switch (level)
    {
    case GCRY_LOG_CONT: level = JNLIB_LOG_CONT; break;
    case GCRY_LOG_INFO: level = JNLIB_LOG_INFO; break;
    case GCRY_LOG_WARN: level = JNLIB_LOG_WARN; break;
    case GCRY_LOG_ERROR:level = JNLIB_LOG_ERROR; break;
    case GCRY_LOG_FATAL:level = JNLIB_LOG_FATAL; break;
    case GCRY_LOG_BUG:  level = JNLIB_LOG_BUG; break;
    case GCRY_LOG_DEBUG:level = JNLIB_LOG_DEBUG; break;
    default:            level = JNLIB_LOG_ERROR; break;  
    }
  log_logv (level, fmt, arg_ptr);
}


/* Callback from libksba to hash a provided buffer.  Our current
   implementation does only allow SHA-1 for hashing. This may be
   extended by mapping the name, testing for algorithm availibility
   and adjust the length checks accordingly. */
static gpg_error_t 
my_ksba_hash_buffer (void *arg, const char *oid,
                     const void *buffer, size_t length, size_t resultsize,
                     unsigned char *result, size_t *resultlen)
{
  (void)arg;

  if (oid && strcmp (oid, "1.3.14.3.2.26")) 
    return gpg_error (GPG_ERR_NOT_SUPPORTED); 
  if (resultsize < 20)
    return gpg_error (GPG_ERR_BUFFER_TOO_SHORT);
  gcry_md_hash_buffer (2, result, buffer, length); 
  *resultlen = 20;
  return 0;
}


/* Setup the debugging.  With a LEVEL of NULL only the active debug
   flags are propagated to the subsystems.  With LEVEL set, a specific
   set of debug flags is set; thus overriding all flags already
   set. */
static void
set_debug (void)
{
  if (!debug_level)
    ;
  else if (!strcmp (debug_level, "none"))
    opt.debug = 0;
  else if (!strcmp (debug_level, "basic"))
    opt.debug = DBG_ASSUAN_VALUE;
  else if (!strcmp (debug_level, "advanced"))
    opt.debug = (DBG_ASSUAN_VALUE|DBG_X509_VALUE|DBG_LOOKUP_VALUE);
  else if (!strcmp (debug_level, "expert"))
    opt.debug = (DBG_ASSUAN_VALUE|DBG_X509_VALUE|DBG_LOOKUP_VALUE
                 |DBG_CACHE_VALUE|DBG_CRYPTO_VALUE);
  else if (!strcmp (debug_level, "guru"))
    opt.debug = ~0;
  else
    {
      log_error (_("invalid debug-level `%s' given\n"), debug_level);
      log_info (_("valid debug levels are: %s\n"),
                "none, basic, advanced, expert, guru");
      opt.debug = 0; /* Reset debugging, so that prior debug
                        statements won't have an undesired effect. */
    }


  if (opt.debug && !opt.verbose)
    {
      opt.verbose = 1;
      gcry_control (GCRYCTL_SET_VERBOSITY, (int)opt.verbose);
    }
  if (opt.debug && opt.quiet)
    opt.quiet = 0;

  if (opt.debug & DBG_CRYPTO_VALUE )
    gcry_control (GCRYCTL_SET_DEBUG_FLAGS, 1);
}
 

static void
i18n_init(void)
{
#ifdef USE_SIMPLE_GETTEXT
  set_gettext_file (PACKAGE);
#else
# ifdef ENABLE_NLS
  setlocale (LC_ALL, "" );
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
# endif
#endif
}


static void
wrong_args (const char *text)
{
  fputs (_("usage: dirmngr [options] "), stderr);
  fputs (text, stderr);
  putc ('\n', stderr);
  dirmngr_exit (2);
}


/* Helper to start the reaper thread for the ldap wrapper.  */
static void
launch_reaper_thread (void)
{
  static int done;
  pth_attr_t tattr;

  if (done)
    return;
  done = 1;

  tattr = pth_attr_new();
  pth_attr_set (tattr, PTH_ATTR_JOINABLE, 0);
  pth_attr_set (tattr, PTH_ATTR_STACK_SIZE, 256*1024);
  pth_attr_set (tattr, PTH_ATTR_NAME, "ldap-reaper");

  if (!pth_spawn (tattr, ldap_wrapper_thread, NULL))
    {
      log_error (_("error spawning ldap wrapper reaper thread: %s\n"),
                 strerror (errno) );
      dirmngr_exit (1);
    }
  pth_attr_destroy (tattr);
}

/* Helper to stop the reaper thread for the ldap wrapper.  */
static void
shutdown_reaper (void)
{
  ldap_wrapper_wait_connections ();
}

/* Handle options which are allowed to be reset after program start.
   Return true if the current option in PARGS could be handled and
   false if not.  As a special feature, passing a value of NULL for
   PARGS, resets the options to the default.  REREAD should be set
   true if it is not the initial option parsing. */
static int
parse_rereadable_options (ARGPARSE_ARGS *pargs, int reread)
{
  if (!pargs)
    { /* Reset mode. */
      opt.quiet = 0;
      opt.verbose = 0;
      opt.debug = 0;
      opt.ldap_wrapper_program = NULL;
      opt.disable_http = 0;
      opt.disable_ldap = 0; 
      opt.honor_http_proxy = 0; 
      opt.http_proxy = NULL; 
      opt.ldap_proxy = NULL; 
      opt.only_ldap_proxy = 0;
      opt.ignore_http_dp = 0;
      opt.ignore_ldap_dp = 0;
      opt.ignore_ocsp_service_url = 0;
      opt.allow_ocsp = 0;
      opt.ocsp_responder = NULL;
      opt.ocsp_max_clock_skew = 10 * 60;      /* 10 minutes.  */
      opt.ocsp_max_period = 90 * 86400;       /* 90 days.  */
      opt.ocsp_current_period = 3 * 60 * 60;  /* 3 hours. */
      opt.max_replies = DEFAULT_MAX_REPLIES;
      while (opt.ocsp_signer)
        {
          fingerprint_list_t tmp = opt.ocsp_signer->next;
          xfree (opt.ocsp_signer);
          opt.ocsp_signer = tmp;
        }
      return 1;
    }

  switch (pargs->r_opt)
    {
    case oQuiet:   opt.quiet = 1; break;
    case oVerbose: opt.verbose++; break;
    case oDebug:   opt.debug |= pargs->r.ret_ulong; break;
    case oDebugAll: opt.debug = ~0; break;
    case oDebugLevel: debug_level = pargs->r.ret_str; break;

    case oLogFile:
      if (!reread)
        return 0; /* Not handled. */
      if (!current_logfile || !pargs->r.ret_str
          || strcmp (current_logfile, pargs->r.ret_str))
        {
          log_set_file (pargs->r.ret_str);
          xfree (current_logfile);
          current_logfile = xtrystrdup (pargs->r.ret_str);
        }
      break;

    case oLDAPWrapperProgram:
      opt.ldap_wrapper_program = pargs->r.ret_str;
      break;
    case oHTTPWrapperProgram:
      opt.http_wrapper_program = pargs->r.ret_str;
      break;

    case oDisableHTTP: opt.disable_http = 1; break;
    case oDisableLDAP: opt.disable_ldap = 1; break;
    case oHonorHTTPProxy: opt.honor_http_proxy = 1; break;
    case oHTTPProxy: opt.http_proxy = pargs->r.ret_str; break;
    case oLDAPProxy: opt.ldap_proxy = pargs->r.ret_str; break;
    case oOnlyLDAPProxy: opt.only_ldap_proxy = 1; break;
    case oIgnoreHTTPDP: opt.ignore_http_dp = 1; break;
    case oIgnoreLDAPDP: opt.ignore_ldap_dp = 1; break;
    case oIgnoreOCSPSvcUrl: opt.ignore_ocsp_service_url = 1; break;

    case oAllowOCSP: opt.allow_ocsp = 1; break;
    case oOCSPResponder: opt.ocsp_responder = pargs->r.ret_str; break;
    case oOCSPSigner: 
      opt.ocsp_signer = parse_ocsp_signer (pargs->r.ret_str);
      break;
    case oOCSPMaxClockSkew: opt.ocsp_max_clock_skew = pargs->r.ret_int; break;
    case oOCSPMaxPeriod: opt.ocsp_max_period = pargs->r.ret_int; break;
    case oOCSPCurrentPeriod: opt.ocsp_current_period = pargs->r.ret_int; break;

    case oMaxReplies: opt.max_replies = pargs->r.ret_int; break;

    default:
      return 0; /* Not handled. */
    }

  return 1; /* Handled. */
}


#ifdef HAVE_W32_SYSTEM
/* The global status of our service.  */
SERVICE_STATUS_HANDLE service_handle;
SERVICE_STATUS service_status;

DWORD WINAPI
w32_service_control (DWORD control, DWORD event_type, LPVOID event_data,
		     LPVOID context)
{
  /* event_type and event_data are not used here.  */
  switch (control)
    {
    case SERVICE_CONTROL_SHUTDOWN:
      /* For shutdown we will try to force termination.  */
      service_status.dwCurrentState = SERVICE_STOP_PENDING;
      SetServiceStatus (service_handle, &service_status); 
      shutdown_pending = 3;
      break;

    case SERVICE_CONTROL_STOP:
      service_status.dwCurrentState = SERVICE_STOP_PENDING;
      SetServiceStatus (service_handle, &service_status); 
      shutdown_pending = 1;
      break;

    default:
      break;
    }
  return 0;
}
#endif /*HAVE_W32_SYSTEM*/


#ifdef HAVE_W32_SYSTEM
#define main real_main
#endif
int
main (int argc, char **argv)
{
#ifdef HAVE_W32_SYSTEM
#undef main
#endif
  enum cmd_and_opt_values cmd = 0;
  ARGPARSE_ARGS pargs;
  int orig_argc;
  char **orig_argv;
  FILE *configfp = NULL;
  char *configname = NULL;
  const char *shell;
  unsigned configlineno;
  int parse_debug = 0;
  int default_config =1;
  int greeting = 0;
  int nogreeting = 0;
  int nodetach = 0;
  int csh_style = 0;
  char *logfile = NULL;
  char *ldapfile = NULL;
  int debug_wait = 0;
  int rc;
  int homedir_seen = 0;

#ifdef HAVE_W32_SYSTEM
  /* The option will be set by main() below if we should run as a
     system daemon.  */
  if (opt.system_service)
    {
      service_handle
	= RegisterServiceCtrlHandlerEx ("DirMngr",
					&w32_service_control, NULL /*FIXME*/);
      if (service_handle == 0)
	log_error ("failed to register service control handler: ec=%d",
		   (int) GetLastError ());
      service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
      service_status.dwCurrentState = SERVICE_START_PENDING;
      service_status.dwControlsAccepted
	= SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
      service_status.dwWin32ExitCode = NO_ERROR;
      service_status.dwServiceSpecificExitCode = NO_ERROR;
      service_status.dwCheckPoint = 0;
      service_status.dwWaitHint = 10000; /* 10 seconds timeout.  */
      SetServiceStatus (service_handle, &service_status); 
    }
#endif /*HAVE_W32_SYSTEM*/

  set_strusage (my_strusage);
  log_set_prefix ("dirmngr", 1|4); 
  
  /* Check that the libraries are suitable.  Do it here because
     the option parsing may need services of the libraries. */

  /* Libgcrypt requires us to register the threading model first.
     Note that this will also do the pth_init. */

  /* Init Libgcrypt. */
  rc = gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pth);
  if (rc)
    {
      log_fatal ("can't register GNU Pth with Libgcrypt: %s\n",
                 gpg_strerror (rc));
    }
  gcry_control (GCRYCTL_DISABLE_SECMEM, 0);
  if (!gcry_check_version (NEED_LIBGCRYPT_VERSION) )
    {
      log_fatal (_("%s is too old (need %s, have %s)\n"),
                 "libgcrypt",
                 NEED_LIBGCRYPT_VERSION, gcry_check_version (NULL) );
    }
  gcry_set_log_handler (my_gcry_logger, NULL);
  gcry_control (GCRYCTL_INITIALIZATION_FINISHED);

  /* Init KSBA.  */
  if (!ksba_check_version (NEED_KSBA_VERSION) )
    {
      log_fatal( _("%s is too old (need %s, have %s)\n"),
                 "libksba",
                 NEED_KSBA_VERSION, ksba_check_version (NULL) );
    }
  ksba_set_malloc_hooks (gcry_malloc, gcry_realloc, gcry_free );
  ksba_set_hash_buffer_function (my_ksba_hash_buffer, NULL);


  /* Init Assuan. */
  assuan_set_malloc_hooks (gcry_malloc, gcry_realloc, gcry_free);
  assuan_set_assuan_log_stream (log_get_stream ());
  assuan_set_assuan_log_prefix (log_get_prefix (NULL));
  assuan_set_assuan_err_source (GPG_ERR_SOURCE_DEFAULT);

  /* Setup I18N. */
  i18n_init();

  /* Setup defaults. */
  shell = getenv ("SHELL");
  if (shell && strlen (shell) >= 3 && !strcmp (shell+strlen (shell)-3, "csh") )
    csh_style = 1;
  
  opt.homedir = default_homedir ();

  /* Reset rereadable options to default values. */
  parse_rereadable_options (NULL, 0); 

  /* LDAP defaults.  */
  opt.add_new_ldapservers = 0;
  opt.ldaptimeout = DEFAULT_LDAP_TIMEOUT;

  /* Other defaults.  */
  socket_name = default_socket_name ();

  /* Check whether we have a config file given on the commandline */
  orig_argc = argc;
  orig_argv = argv;
  pargs.argc = &argc;
  pargs.argv = &argv;
  pargs.flags= 1|(1<<6);  /* do not remove the args, ignore version */
  while (arg_parse( &pargs, opts))
    {
      if (pargs.r_opt == oDebug || pargs.r_opt == oDebugAll)
        parse_debug++;
      else if (pargs.r_opt == oOptions)
        { /* Yes there is one, so we do not try the default one, but
	     read the option file when it is encountered at the
	     commandline */
          default_config = 0;
	}
      else if (pargs.r_opt == oNoOptions)
        default_config = 0; /* --no-options */
      else if (pargs.r_opt == oHomedir)
        {
          opt.homedir = pargs.r.ret_str;
          homedir_seen = 1;
        }
      else if (pargs.r_opt == aDaemon)
        opt.system_daemon = 1;
      else if (pargs.r_opt == aService)
        {
	  /* Redundant.  The main function takes care of it.  */
	  opt.system_service = 1;
	  opt.system_daemon = 1;
	}
#ifdef HAVE_W32_SYSTEM
      else if (pargs.r_opt == aGPGConfList || pargs.r_opt == aGPGConfTest)
	/* We set this so we switch to the system configuration
	   directory below.  This is a crutch to solve the problem
	   that the user configuration is never used on Windows.  Also
	   see below at aGPGConfList.  */
        opt.system_daemon = 1;
#endif
    }

  /* If --daemon has been given on the command line but not --homedir,
     we switch to /etc/dirmngr as default home directory.  Note, that
     this also overrides the GNUPGHOME environment variable.  */
  if (opt.system_daemon && !homedir_seen)
    {
      opt.homedir = dirmngr_sysconfdir ();
      opt.homedir_data = dirmngr_datadir ();
      opt.homedir_cache = dirmngr_cachedir ();
    }

  if (default_config)
    configname = make_filename (opt.homedir, "dirmngr.conf", NULL );
  
  argc = orig_argc;
  argv = orig_argv;
  pargs.argc = &argc;
  pargs.argv = &argv;
  pargs.flags= 1;  /* do not remove the args */
 next_pass:
  if (configname)
    {
      configlineno = 0;
      configfp = fopen (configname, "r");
      if (!configfp)
        {
          if (default_config)
            {
              if( parse_debug )
                log_info (_("NOTE: no default option file `%s'\n"),
                          configname );
	    }
          else
            {
              log_error (_("option file `%s': %s\n"),
                         configname, strerror(errno) );
              exit(2);
	    }
          xfree (configname); 
          configname = NULL;
	}
      if (parse_debug && configname )
        log_info (_("reading options from `%s'\n"), configname );
      default_config = 0;
    }

  while (optfile_parse( configfp, configname, &configlineno, &pargs, opts) )
    {
      if (parse_rereadable_options (&pargs, 0))
        continue; /* Already handled */
      switch (pargs.r_opt)
        {
        case aServer: 
        case aDaemon:
        case aService:
        case aShutdown: 
        case aFlush: 
	case aListCRLs: 
	case aLoadCRL: 
        case aFetchCRL:
	case aGPGConfList:
	case aGPGConfTest:
          cmd = pargs.r_opt;
          break;

        case oQuiet: opt.quiet = 1; break;
        case oVerbose: opt.verbose++; break;
        case oBatch: opt.batch=1; break;

        case oDebug: opt.debug |= pargs.r.ret_ulong; break;
        case oDebugAll: opt.debug = ~0; break;
        case oDebugLevel: debug_level = pargs.r.ret_str; break;
        case oDebugWait: debug_wait = pargs.r.ret_int; break;

        case oOptions:
          /* Config files may not be nested (silently ignore them) */
          if (!configfp)
            {
		xfree(configname);
		configname = xstrdup(pargs.r.ret_str);
		goto next_pass;
	    }
          break;
        case oNoGreeting: nogreeting = 1; break;
        case oNoVerbose: opt.verbose = 0; break;
        case oNoOptions: break; /* no-options */
        case oHomedir: /* Ignore this option here. */; break;
        case oNoDetach: nodetach = 1; break;
        case oLogFile: logfile = pargs.r.ret_str; break;
        case oCsh: csh_style = 1; break;
        case oSh: csh_style = 0; break;
	case oLDAPFile: ldapfile = pargs.r.ret_str; break;
	case oLDAPAddServers: opt.add_new_ldapservers = 1; break;
	case oLDAPTimeout: 
	  opt.ldaptimeout = pargs.r.ret_int; 
	  break;

        case oFakedSystemTime:
          set_time ( (time_t)pargs.r.ret_ulong, 0);
          break;

        case oForce: opt.force = 1; break;

        case oSocketName: socket_name = pargs.r.ret_str; break;

        default : pargs.err = configfp? 1:2; break;
	}
    }
  if (configfp)
    {
      fclose( configfp );
      configfp = NULL;
      /* Keep a copy of the name so that it can be read on SIGHUP. */
      opt.config_filename = configname;
      configname = NULL;
      goto next_pass;
    }
  xfree (configname);
  configname = NULL;
  if (log_get_errorcount(0))
    exit(2);
  if (nogreeting )
    greeting = 0;

  if (!opt.homedir_data)
    opt.homedir_data = opt.homedir;
  if (!opt.homedir_cache)
    opt.homedir_cache = opt.homedir;

  if (greeting)
    {
      fprintf (stderr, "%s %s; %s\n",
               strusage(11), strusage(13), strusage(14) );
      fprintf (stderr, "%s\n", strusage(15) );
    }

#ifdef IS_DEVELOPMENT_VERSION
  log_info ("NOTE: this is a development version!\n");
#endif

  if (faked_time_p ())
    {
      dirmngr_isotime_t tbuf;
      get_isotime (tbuf);
      log_info (_("WARNING: running with faked system time %s\n"), tbuf);
    }

  set_debug ();

  /* Get LDAP server list from file. */
  if (!ldapfile) 
    {
      ldapfile = make_filename (opt.homedir,
                                opt.system_daemon?
                                "ldapservers.conf":"dirmngr_ldapservers.conf",
                                NULL);
      opt.ldapservers = parse_ldapserver_file (ldapfile);
      xfree (ldapfile);
    }
  else
      opt.ldapservers = parse_ldapserver_file (ldapfile);

#ifndef HAVE_W32_SYSTEM
  /* We need to ignore the PIPE signal because the we might log to a
     socket and that code handles EPIPE properly.  The ldap wrapper
     also requires us to ignore this silly signal. Assuan would set
     this signal to ignore anyway.*/
  signal (SIGPIPE, SIG_IGN);
#endif

  /* Ready.  Now to our duties. */
  if (!cmd && opt.system_service)
    cmd = aDaemon;
  else if (!cmd)
    cmd = aServer;
  rc = 0;

  if (cmd == aServer)
    {
      if (argc)
        wrong_args ("--server");

      if (logfile)
        {
          log_set_file (logfile);
          log_set_prefix (NULL, 2|4);
	  assuan_set_assuan_log_stream (log_get_stream ());
        }
       
      if (debug_wait)
        {
          log_debug ("waiting for debugger - my pid is %u .....\n",
                     (unsigned int)getpid());
          sleep (debug_wait);
          log_debug ("... okay\n");
        }

      launch_reaper_thread ();
      cert_cache_init ();
      crl_cache_init ();
      start_command_handler (ASSUAN_INVALID_FD);
      shutdown_reaper ();
    }
  else if (cmd == aDaemon)
    {
      assuan_fd_t fd;
      pid_t pid;
      int len;
      struct sockaddr_un serv_addr;

      if (argc)
        wrong_args ("--daemon");
      
      /* Now start with logging to a file if this is desired. */
      if (logfile)
        {
          log_set_file (logfile);
          log_set_prefix (NULL, (JNLIB_LOG_WITH_PREFIX
                                 |JNLIB_LOG_WITH_TIME
                                 |JNLIB_LOG_WITH_PID));
          current_logfile = xstrdup (logfile);
	  assuan_set_assuan_log_stream (log_get_stream ());
        }

#ifndef HAVE_W32_SYSTEM
      if (strchr (socket_name, ':'))
        {
          log_error (_("colons are not allowed in the socket name\n"));
          dirmngr_exit (1);
        }
#endif
      if (strlen (socket_name)+1 >= sizeof serv_addr.sun_path ) 
        {
          log_error (_("name of socket too long\n"));
          dirmngr_exit (1);
        }
    
      fd = assuan_sock_new (AF_UNIX, SOCK_STREAM, 0);
      if (fd == ASSUAN_INVALID_FD)
        {
          log_error (_("can't create socket: %s\n"), strerror (errno));
          cleanup ();
          dirmngr_exit (1);
        }

      memset (&serv_addr, 0, sizeof serv_addr);
      serv_addr.sun_family = AF_UNIX;
      strcpy (serv_addr.sun_path, socket_name);
      len = (offsetof (struct sockaddr_un, sun_path)
             + strlen (serv_addr.sun_path) + 1);

      rc = assuan_sock_bind (fd, (struct sockaddr*) &serv_addr, len);
      if (rc == -1 && errno == EADDRINUSE)
	{
	  remove (socket_name);
	  rc = assuan_sock_bind (fd, (struct sockaddr*) &serv_addr, len);
	}
      if (rc != -1 
	  && (rc = assuan_sock_get_nonce ((struct sockaddr*) &serv_addr, len, &socket_nonce)))
	log_error (_("error getting nonce for the socket\n"));
      if (rc == -1)
        {
          log_error (_("error binding socket to `%s': %s\n"),
                     serv_addr.sun_path, gpg_strerror (gpg_error_from_errno (errno)));
          assuan_sock_close (fd);
          dirmngr_exit (1);
        }
      cleanup_socket = 1;
  
      if (listen (FD2INT (fd), 5) == -1)
        {
          log_error (_("listen() failed: %s\n"), strerror (errno));
          assuan_sock_close (fd);
          dirmngr_exit (1);
        }

      if (opt.verbose)
        log_info (_("listening on socket `%s'\n"), socket_name );

      fflush (NULL);

#ifdef HAVE_W32_SYSTEM
      pid = getpid ();
      printf ("set DIRMNGR_INFO=%s;%lu;1\n", socket_name, (ulong) pid);
#else
      pid = pth_fork ();
      if (pid == (pid_t)-1) 
        {
          log_fatal (_("fork failed: %s\n"), strerror (errno) );
          dirmngr_exit (1);
        }

      if (pid) 
        { /* We are the parent */
          char *infostr;
          
          /* Don't let cleanup() remove the socket - the child is
             responsible for doing that.  */
          cleanup_socket = 0;

          close (fd);
          
          /* Create the info string: <name>:<pid>:<protocol_version> */
          if (asprintf (&infostr, "DIRMNGR_INFO=%s:%lu:1",
                        socket_name, (ulong)pid ) < 0)
            {
              log_error (_("out of core\n"));
              kill (pid, SIGTERM);
              dirmngr_exit (1);
            }
          /* Print the environment string, so that the caller can use
             shell's eval to set it */
          if (csh_style)
            {
              *strchr (infostr, '=') = ' ';
              printf ( "setenv %s\n", infostr);
            }
          else
            {
              printf ( "%s; export DIRMNGR_INFO;\n", infostr);
            }
          free (infostr);
          exit (0); 
          /*NEVER REACHED*/
        } /* end parent */
      
      
      /* 
         This is the child
       */

      /* Detach from tty and put process into a new session */
      if (!nodetach )
        { 
          int i;
          unsigned int oldflags;

          /* Close stdin, stdout and stderr unless it is the log stream */
          for (i=0; i <= 2; i++)
            {
              if (!log_test_fd (i) && i != fd )
                close (i);
            }
          if (setsid() == -1)
            {
              log_error (_("setsid() failed: %s\n"), strerror(errno) );
              dirmngr_exit (1);
            }

          log_get_prefix (&oldflags);
          log_set_prefix (NULL, oldflags | JNLIB_LOG_RUN_DETACHED);
          opt.running_detached = 1;

          if (chdir("/"))
            {
              log_error (_("chdir to / failed: %s\n"), strerror (errno));
              dirmngr_exit (1);
            }
        }
#endif

      launch_reaper_thread ();
      cert_cache_init ();
      crl_cache_init ();
#ifdef HAVE_W32_SYSTEM
      if (opt.system_service)
	{
	  service_status.dwCurrentState = SERVICE_RUNNING;
	  SetServiceStatus (service_handle, &service_status);
	}
#endif
      handle_connections (fd);
      assuan_sock_close (fd);
      shutdown_reaper ();
#ifdef HAVE_W32_SYSTEM
      if (opt.system_service)
	{
	  service_status.dwCurrentState = SERVICE_STOPPED;
	  SetServiceStatus (service_handle, &service_status);
	}
#endif
    }
  else if (cmd == aListCRLs)
    {
      /* Just list the CRL cache and exit. */
      if (argc)
        wrong_args ("--list-crls");
      launch_reaper_thread ();
      crl_cache_init ();
      crl_cache_list (stdout);
    }
  else if (cmd == aLoadCRL)
    {
      struct server_control_s ctrlbuf;

      memset (&ctrlbuf, 0, sizeof ctrlbuf);
      dirmngr_init_default_ctrl (&ctrlbuf);

      launch_reaper_thread ();
      cert_cache_init ();
      crl_cache_init ();
      if (!argc)
        rc = crl_cache_load (&ctrlbuf, NULL);
      else
        {
          for (; !rc && argc; argc--, argv++)
            rc = crl_cache_load (&ctrlbuf, *argv);
        }
    }
  else if (cmd == aFetchCRL)
    {
      ksba_reader_t reader;
      struct server_control_s ctrlbuf;

      if (argc != 1)
        wrong_args ("--fetch-crl URL");

      memset (&ctrlbuf, 0, sizeof ctrlbuf);
      dirmngr_init_default_ctrl (&ctrlbuf);

      launch_reaper_thread ();
      cert_cache_init ();
      crl_cache_init ();
      rc = crl_fetch (&ctrlbuf, argv[0], &reader);
      if (rc)
        log_error (_("fetching CRL from `%s' failed: %s\n"),
                     argv[0], gpg_strerror (rc));
      else
        {
          rc = crl_cache_insert (&ctrlbuf, argv[0], reader); 
          if (rc)
            log_error (_("processing CRL from `%s' failed: %s\n"),
                       argv[0], gpg_strerror (rc));
          crl_close_reader (reader);
        }
    }
  else if (cmd == aFlush)
    {
      /* Delete cache and exit. */
      if (argc)
        wrong_args ("--flush");
      rc = crl_cache_flush();
    }
  else if (cmd == aGPGConfTest)
    dirmngr_exit (0);
  else if (cmd == aGPGConfList)
    {
      unsigned long flags = 0;
      char *filename;
      char *filename_esc;

      /* List options and default values in the GPG Conf format.  */

/* The following list is taken from gnupg/tools/gpgconf-comp.c.  */
/* Option flags.  YOU MUST NOT CHANGE THE NUMBERS OF THE EXISTING
   FLAGS, AS THEY ARE PART OF THE EXTERNAL INTERFACE.  */
#define GC_OPT_FLAG_NONE	0UL
/* The DEFAULT flag for an option indicates that the option has a
   default value.  */
#define GC_OPT_FLAG_DEFAULT	(1UL << 4)
/* The DEF_DESC flag for an option indicates that the option has a
   default, which is described by the value of the default field.  */
#define GC_OPT_FLAG_DEF_DESC	(1UL << 5)
/* The NO_ARG_DESC flag for an option indicates that the argument has
   a default, which is described by the value of the ARGDEF field.  */
#define GC_OPT_FLAG_NO_ARG_DESC	(1UL << 6)
#define GC_OPT_FLAG_NO_CHANGE   (1UL <<7)

#ifdef HAVE_W32_SYSTEM
      /* On Windows systems, dirmngr always runs as system daemon, and
	 the per-user configuration is never used.  So we short-cut
	 everything to use the global system configuration of dirmngr
	 above, and here we set the no change flag to make these
	 read-only.  */
      flags |= GC_OPT_FLAG_NO_CHANGE;
#endif

      /* First the configuration file.  This is not an option, but it
	 is vital information for GPG Conf.  */
      if (!opt.config_filename)
        opt.config_filename = make_filename (opt.homedir,
                                             "dirmngr.conf", NULL );

      filename = percent_escape (opt.config_filename, NULL);
      printf ("gpgconf-dirmngr.conf:%lu:\"%s\n",
              GC_OPT_FLAG_DEFAULT, filename);
      xfree (filename);

      printf ("verbose:%lu:\n", flags | GC_OPT_FLAG_NONE);
      printf ("quiet:%lu:\n", flags | GC_OPT_FLAG_NONE);
      printf ("debug-level:%lu:\"none\n", flags | GC_OPT_FLAG_DEFAULT);
      printf ("log-file:%lu:\n", flags | GC_OPT_FLAG_NONE);
      printf ("force:%lu:\n", flags | GC_OPT_FLAG_NONE);

      /* --csh and --sh are mutually exclusive, something we can not
         express in GPG Conf.  --options is only usable from the
         command line, really.  --debug-all interacts with --debug,
         and having both of them is thus problematic.  --no-detach is
         also only usable on the command line.  --batch is unused.  */

      filename = make_filename (opt.homedir, 
                                opt.system_daemon?
                                "ldapservers.conf":"dirmngr_ldapservers.conf",
                                NULL);
      filename_esc = percent_escape (filename, NULL);
      printf ("ldapserverlist-file:%lu:\"%s\n", flags | GC_OPT_FLAG_DEFAULT,
	      filename_esc);
      xfree (filename_esc);
      xfree (filename);

      printf ("ldaptimeout:%lu:%u\n",
              flags | GC_OPT_FLAG_DEFAULT, DEFAULT_LDAP_TIMEOUT);
      printf ("max-replies:%lu:%u\n",
              flags | GC_OPT_FLAG_DEFAULT, DEFAULT_MAX_REPLIES);
      printf ("allow-ocsp:%lu:\n", flags | GC_OPT_FLAG_NONE);
      printf ("ocsp-responder:%lu:\n", flags | GC_OPT_FLAG_NONE);
      printf ("ocsp-signer:%lu:\n", flags | GC_OPT_FLAG_NONE);

      printf ("faked-system-time:%lu:\n", flags | GC_OPT_FLAG_NONE);
      printf ("no-greeting:%lu:\n", flags | GC_OPT_FLAG_NONE);

      printf ("disable-http:%lu:\n", flags | GC_OPT_FLAG_NONE);
      printf ("disable-ldap:%lu:\n", flags | GC_OPT_FLAG_NONE);
      printf ("honor-http-proxy:%lu\n", flags | GC_OPT_FLAG_NONE);
      printf ("http-proxy:%lu:\n", flags | GC_OPT_FLAG_NONE);
      printf ("ldap-proxy:%lu:\n", flags | GC_OPT_FLAG_NONE);
      printf ("only-ldap-proxy:%lu:\n", flags | GC_OPT_FLAG_NONE);
      printf ("ignore-ldap-dp:%lu:\n", flags | GC_OPT_FLAG_NONE);
      printf ("ignore-http-dp:%lu:\n", flags | GC_OPT_FLAG_NONE);
      printf ("ignore-ocsp-service-url:%lu:\n", flags | GC_OPT_FLAG_NONE);
      /* Note: The next one is to fix a typo in gpgconf - should be
         removed eventually. */
      printf ("ignore-ocsp-servic-url:%lu:\n", flags | GC_OPT_FLAG_NONE);
    }
  cleanup ();
  return !!rc;
}


#ifdef HAVE_W32_SYSTEM
int
main (int argc, char *argv[])
{
  int i;

  /* Find out if we run in daemon mode or on the command line.  */
  for (i = 1; i < argc; i++)
    if (!strcmp (argv[i], "--service"))
      {
	opt.system_service = 1;
	opt.system_daemon = 1;
	break;
      }

  if (!opt.system_service)
    return real_main (argc, argv);
  else
    {
      SERVICE_TABLE_ENTRY DispatchTable [] =
	{
	  /* Ignore warning.  */
	  { "DirMngr", &real_main },
	  { NULL, NULL }
	};

      if (!StartServiceCtrlDispatcher (DispatchTable))
        return 1;
      return 0;
    }
}
#endif


static void
cleanup (void)
{
  crl_cache_deinit ();
  cert_cache_deinit (1);

  ldapserver_list_free (opt.ldapservers);
  opt.ldapservers = NULL;

  if (cleanup_socket)
    {
      cleanup_socket = 0;
      if (socket_name && *socket_name)
        remove (socket_name);
    }
}


void 
dirmngr_exit (int rc)
{
  cleanup ();
  exit (rc);
}


void
dirmngr_init_default_ctrl (ctrl_t ctrl)
{
  (void)ctrl;

  /* Nothing for now. */
}


/* Create a list of LDAP servers from the file FILENAME. Returns the
   list or NULL in case of errors. 

   The format fo such a file is line oriented where empty lines and
   lines starting with a hash mark are ignored.  All other lines are
   assumed to be colon seprated with these fields:

   1. field: Hostname
   2. field: Portnumber
   3. field: Username 
   4. field: Password
   5. field: Base DN

*/
static ldap_server_t
parse_ldapserver_file (const char* filename)
{
  char buffer[1024];
  char *p;
  ldap_server_t server, serverstart, *serverend;
  int c;
  unsigned int lineno = 0;
  FILE *fp;

  fp = fopen (filename, "r");
  if (!fp)
    {
      log_error (_("error opening `%s': %s\n"), filename, strerror (errno));
      return NULL;
    }

  serverstart = NULL;
  serverend = &serverstart;
  while (fgets (buffer, sizeof buffer, fp))
    {
      lineno++;
      if (!*buffer || buffer[strlen(buffer)-1] != '\n')
        {
          if (*buffer && feof (fp))
            ; /* Last line not terminated - continue. */
          else
            {
              log_error (_("%s:%u: line too long - skipped\n"),
                         filename, lineno);
              while ( (c=fgetc (fp)) != EOF && c != '\n')
                ; /* Skip until end of line. */
              continue;
            }
        }
      /* Skip empty and comment lines.*/
      for (p=buffer; spacep (p); p++)
        ;
      if (!*p || *p == '\n' || *p == '#')
        continue;

      /* Parse the colon separated fields. */
      server = ldapserver_parse_one (buffer, filename, lineno);
      if (server)
        {
          *serverend = server;
          serverend = &server->next;
        }
    } 
  
  if (ferror (fp))
    log_error (_("error reading `%s': %s\n"), filename, strerror (errno));
  fclose (fp);

  return serverstart;
}


static fingerprint_list_t
parse_ocsp_signer (const char *string)
{
  gpg_error_t err;
  char *fname;
  FILE *fp;
  char line[256];
  char *p;
  fingerprint_list_t list, *list_tail, item;
  unsigned int lnr = 0;
  int c, i, j;
  int errflag = 0;


  /* Check whether this is not a filename and treat it as a direct
     fingerprint specification.  */
  if (!strpbrk (string, "/.~\\"))
    {
      item = xcalloc (1, sizeof *item);
      for (i=j=0; (string[i] == ':' || hexdigitp (string+i)) && j < 40; i++)
        if ( string[i] != ':' )
          item->hexfpr[j++] = string[i] >= 'a'? (string[i] & 0xdf): string[i];
      item->hexfpr[j] = 0;
      if (j != 40 || !(spacep (string+i) || !string[i]))
        {
          log_error (_("%s:%u: invalid fingerprint detected\n"), 
                     "--ocsp-signer", 0);
          xfree (item);
          return NULL;
        }
      return item;
    }
  
  /* Well, it is a filename.  */
  if (*string == '/' || (*string == '~' && string[1] == '/'))
    fname = make_filename (string, NULL);
  else
    {
      if (string[0] == '.' && string[1] == '/' )
        string += 2;
      fname = make_filename (opt.homedir, string, NULL);
    }

  fp = fopen (fname, "r");
  if (!fp)
    {
      err = gpg_error_from_syserror ();
      log_error (_("can't open `%s': %s\n"), fname, gpg_strerror (err));
      xfree (fname);
      return NULL;
    }

  list = NULL;
  list_tail = &list;
  for (;;)
    {
      if (!fgets (line, DIM(line)-1, fp) )
        {
          if (!feof (fp))
            {
              err = gpg_error_from_syserror ();
              log_error (_("%s:%u: read error: %s\n"),
                         fname, lnr, gpg_strerror (err));
              errflag = 1;
            }
          fclose (fp);
          if (errflag)
            {
              while (list)
                {
                  fingerprint_list_t tmp = list->next;
                  xfree (list);
                  list = tmp;
                }
            }
          xfree (fname);
          return list; /* Ready.  */
        }

      lnr++;
      if (!*line || line[strlen(line)-1] != '\n')
        {
          /* Eat until end of line. */
          while ( (c=getc (fp)) != EOF && c != '\n')
            ;
          err = gpg_error (*line? GPG_ERR_LINE_TOO_LONG
                           /* */: GPG_ERR_INCOMPLETE_LINE);
          log_error (_("%s:%u: read error: %s\n"), 
                     fname, lnr, gpg_strerror (err));
          errflag = 1;
          continue;
        }

      /* Allow for empty lines and spaces */
      for (p=line; spacep (p); p++)
        ;
      if (!*p || *p == '\n' || *p == '#')
        continue;

      item = xcalloc (1, sizeof *item);
      *list_tail = item;
      list_tail = &item->next;

      for (i=j=0; (p[i] == ':' || hexdigitp (p+i)) && j < 40; i++)
        if ( p[i] != ':' )
          item->hexfpr[j++] = p[i] >= 'a'? (p[i] & 0xdf): p[i];
      item->hexfpr[j] = 0;
      if (j != 40 || !(spacep (p+i) || p[i] == '\n'))
        {
          log_error (_("%s:%u: invalid fingerprint detected\n"), fname, lnr);
          errflag = 1;
        }
      i++;
      while (spacep (p+i))
        i++;
      if (p[i] && p[i] != '\n')
        log_info (_("%s:%u: garbage at end of line ignored\n"), fname, lnr);
    }
  /*NOTREACHED*/
}




/*
   Stuff used in daemon mode.  
 */



/* Reread parts of the configuration.  Note, that this function is
   obviously not thread-safe and should only be called from the PTH
   signal handler. 

   Fixme: Due to the way the argument parsing works, we create a
   memory leak here for all string type arguments.  There is currently
   no clean way to tell whether the memory for the argument has been
   allocated or points into the process' original arguments.  Unless
   we have a mechanism to tell this, we need to live on with this. */
static void
reread_configuration (void)
{
  ARGPARSE_ARGS pargs;
  FILE *fp;
  unsigned int configlineno = 0;
  int dummy;

  if (!opt.config_filename)
    return; /* No config file. */

  fp = fopen (opt.config_filename, "r");
  if (!fp)
    {
      log_error (_("option file `%s': %s\n"),
                 opt.config_filename, strerror(errno) );
      return;
    }

  parse_rereadable_options (NULL, 1); /* Start from the default values. */

  memset (&pargs, 0, sizeof pargs);
  dummy = 0;
  pargs.argc = &dummy;
  pargs.flags = 1;  /* do not remove the args */
  while (optfile_parse (fp, opt.config_filename, &configlineno, &pargs, opts) )
    {
      if (pargs.r_opt < -1)
        pargs.err = 1; /* Print a warning. */
      else /* Try to parse this option - ignore unchangeable ones. */
        parse_rereadable_options (&pargs, 1);
    }
  fclose (fp);

  set_debug ();
}



/* The signal handler. */
static void
handle_signal (int signo)
{
  switch (signo)
    {
#ifndef HAVE_W32_SYSTEM
    case SIGHUP:
      log_info (_("SIGHUP received - "
                  "re-reading configuration and flushing caches\n"));
      reread_configuration ();
      cert_cache_deinit (0);
      crl_cache_deinit ();
      cert_cache_init ();
      crl_cache_init ();
      break;
      
    case SIGUSR1:
      cert_cache_print_stats ();
      break;
      
    case SIGUSR2:
      log_info (_("SIGUSR2 received - no action defined\n"));
      break;

    case SIGTERM:
      if (!shutdown_pending)
        log_info (_("SIGTERM received - shutting down ...\n"));
      else
        log_info (_("SIGTERM received - still %d active connections\n"),
                  active_connections);
      shutdown_pending++;
      if (shutdown_pending > 2)
        {
          log_info (_("shutdown forced\n"));
          log_info ("%s %s stopped\n", strusage(11), strusage(13) );
          cleanup ();
          dirmngr_exit (0);
	}
      break;
        
    case SIGINT:
      log_info (_("SIGINT received - immediate shutdown\n"));
      log_info( "%s %s stopped\n", strusage(11), strusage(13));
      cleanup ();
      dirmngr_exit (0);
      break;
#endif
    default:
      log_info (_("signal %d received - no action defined\n"), signo);
    }
}


/* This is the worker for the ticker.  It is called every few seconds
   and may only do fast operations. */
static void
handle_tick (void)
{
  /* Nothing real to do right now.  Actually we need the timeout only
     for W32 where we don't use signals and need a way for the loop to
     check for the shutdown flag. */
#ifdef HAVE_W32_SYSTEM
  if (shutdown_pending)
    log_info (_("SIGTERM received - shutting down ...\n"));
  if (shutdown_pending > 2)
    {
      log_info (_("shutdown forced\n"));
      log_info ("%s %s stopped\n", strusage(11), strusage(13) );
      cleanup ();
      dirmngr_exit (0);
    }
#endif /*HAVE_W32_SYSTEM*/
}


/* Check the nonce on a new connection.  This is a NOP unless we we
   are using our Unix domain socket emulation under Windows.  */
static int 
check_nonce (assuan_fd_t fd, assuan_sock_nonce_t *nonce)
{
  if (assuan_sock_check_nonce (fd, nonce))
    {
      log_info (_("error reading nonce on fd %d: %s\n"), 
                FD2INT (fd), strerror (errno));
      assuan_sock_close (fd);
      return -1;
    }
  else
    return 0;
}


/* Helper to call a connection's main fucntion. */
static void *
start_connection_thread (void *arg)
{
  assuan_fd_t fd = (assuan_fd_t) arg;

  if (check_nonce (fd, &socket_nonce))
    return NULL;

  active_connections++;
  if (opt.verbose)
    log_info (_("handler for fd %d started\n"), FD2INT (fd));

  start_command_handler (fd);

  if (opt.verbose)
    log_info (_("handler for fd %d terminated\n"), FD2INT (fd));
  active_connections--;
  
  return NULL;
}


/* Main loop in daemon mode. */
static void
handle_connections (assuan_fd_t listen_fd)
{
  pth_attr_t tattr;
  pth_event_t ev, time_ev;
  sigset_t sigs, oldsigs;
  int signo;
  struct sockaddr_un paddr;
  socklen_t plen = sizeof( paddr );
  assuan_fd_t fd;

  tattr = pth_attr_new();
  pth_attr_set (tattr, PTH_ATTR_JOINABLE, 0);
  pth_attr_set (tattr, PTH_ATTR_STACK_SIZE, 1024*1024);
  pth_attr_set (tattr, PTH_ATTR_NAME, "dirmngr");

#ifndef HAVE_W32_SYSTEM /* FIXME */
  sigemptyset (&sigs );
  sigaddset (&sigs, SIGHUP);
  sigaddset (&sigs, SIGUSR1);
  sigaddset (&sigs, SIGUSR2);
  sigaddset (&sigs, SIGINT);
  sigaddset (&sigs, SIGTERM);
  ev = pth_event (PTH_EVENT_SIGS, &sigs, &signo);
#else
  sigs = 0;
  ev = pth_event (PTH_EVENT_SIGS, &sigs, &signo);
#endif
  time_ev = NULL;

  for (;;)
    {
      if (shutdown_pending)
        {
          if (!active_connections)
            break; /* ready */

          /* Do not accept anymore connections but wait for existing
             connections to terminate.  */
          signo = 0;
          pth_wait (ev);
          if (pth_event_occurred (ev) && signo)
            handle_signal (signo);
          continue;
	}

      if (!time_ev)
        time_ev = pth_event (PTH_EVENT_TIME, 
                             pth_timeout (TIMERTICK_INTERVAL, 0));

      if (time_ev)
        pth_event_concat (ev, time_ev, NULL);
      fd = (assuan_fd_t) pth_accept_ev (FD2INT (listen_fd), (struct sockaddr *)&paddr, &plen, ev);
      if (time_ev)
        pth_event_isolate (time_ev);

      if (fd == ASSUAN_INVALID_FD)
        {
          if (pth_event_occurred (ev)
              || (time_ev && pth_event_occurred (time_ev)) )
            {
              if (pth_event_occurred (ev))
                handle_signal (signo);
              if (time_ev && pth_event_occurred (time_ev))
                {
                  pth_event_free (time_ev, PTH_FREE_ALL);
                  time_ev = NULL;
                  handle_tick ();
                }
              continue;
	    }
          log_error (_("accept failed: %s - waiting 1s\n"), strerror (errno));
          pth_sleep (1);
          continue;
	}

      if (pth_event_occurred (ev))
        {
          handle_signal (signo);
        }

      if (time_ev && pth_event_occurred (time_ev))
        {
          pth_event_free (time_ev, PTH_FREE_ALL);
          time_ev = NULL;
          handle_tick ();
        }


      /* We now might create a new thread and because we don't want
         any signals (as we are handling them here) to be delivered to
         a new thread we need to block those signals. */
      pth_sigmask (SIG_BLOCK, &sigs, &oldsigs);

      /* Create thread to handle this connection.  */
      if (!pth_spawn (tattr, start_connection_thread, (void*)fd))
        {
          log_error (_("error spawning connection handler: %s\n"),
                     strerror (errno) );
          assuan_sock_close (fd);
	}

      /* Restore the signal mask. */
      pth_sigmask (SIG_SETMASK, &oldsigs, NULL);
    }
  
  pth_event_free (ev, PTH_FREE_ALL);
  if (time_ev)
    pth_event_free (time_ev, PTH_FREE_ALL);
  pth_attr_destroy (tattr);
  cleanup ();
  log_info ("%s %s stopped\n", strusage(11), strusage(13));
}
