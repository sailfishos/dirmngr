/* exechelp.c - fork and exec helpers
 * Copyright (C) 2004, 2007, 2008 g10 Code GmbH
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h> 
#include <fcntl.h>

#include <pth.h>

#ifndef HAVE_W32_SYSTEM
#include <sys/wait.h>
#endif

#include "util.h"
#include "i18n.h"
#include "exechelp.h"

/* Define to 1 do enable debugging.  */
#define DEBUG_W32_SPAWN 0

#ifdef _POSIX_OPEN_MAX
#define MAX_OPEN_FDS _POSIX_OPEN_MAX
#else
#define MAX_OPEN_FDS 20
#endif

#ifdef HAVE_W32_SYSTEM
/* We assume that a HANDLE can be represented by an int which should
   be true for all i386 systems (HANDLE is defined as void *) and
   these are the only systems for which Windows is available.  Further
   we assume that -1 denotes an invalid handle.  */
# define fd_to_handle(a)  ((HANDLE)(a))
# define handle_to_fd(a)  ((int)(a))
# define pid_to_handle(a) ((HANDLE)(a))
# define handle_to_pid(a) ((int)(a))
#endif


#ifdef HAVE_W32_SYSTEM
/* Helper function to build_w32_commandline. */
static char *
build_w32_commandline_copy (char *buffer, const char *string)
{
  char *p = buffer;
  const char *s;

  if (!*string) /* Empty string. */
    p = stpcpy (p, "\"\"");
  else if (strpbrk (string, " \t\n\v\f\""))
    {
      /* Need to do some kind of quoting.  */
      p = stpcpy (p, "\"");
      for (s=string; *s; s++)
        {
          *p++ = *s;
          if (*s == '\"')
            *p++ = *s;
        }
      *p++ = '\"';
      *p = 0;
    }
  else
    p = stpcpy (p, string);

  return p;
}

/* Build a command line for use with W32's CreateProcess.  On success
   CMDLINE gets the address of a newly allocated string.  */
static gpg_error_t
build_w32_commandline (const char *pgmname, const char * const *argv, 
                       char **cmdline)
{
  int i, n;
  const char *s;
  char *buf, *p;

  *cmdline = NULL;
  n = 0;
  s = pgmname;
  n += strlen (s) + 1 + 2;  /* (1 space, 2 quoting) */
  for (; *s; s++)
    if (*s == '\"')
      n++;  /* Need to double inner quotes.  */
  for (i=0; (s=argv[i]); i++)
    {
      n += strlen (s) + 1 + 2;  /* (1 space, 2 quoting) */
      for (; *s; s++)
        if (*s == '\"')
          n++;  /* Need to double inner quotes.  */
    }
  n++;

  buf = p = xtrymalloc (n);
  if (!buf)
    return gpg_error_from_syserror ();

  p = build_w32_commandline_copy (p, pgmname);
  for (i=0; argv[i]; i++) 
    {
      *p++ = ' ';
      p = build_w32_commandline_copy (p, argv[i]);
    }

  *cmdline= buf;
  return 0;
}
#endif /*HAVE_W32_SYSTEM*/


#ifndef HAVE_W32_SYSTEM
/* The exec core used right after the fork. This will never return. */
static void
do_exec (const char *pgmname, char *argv[],
         int fd_in, int fd_out, int fd_err)
{
  char **arg_list;
  int n, i, j;
  int fds[3];

  fds[0] = fd_in;
  fds[1] = fd_out;
  fds[2] = fd_err;

  /* Create the command line argument array.  */
  i = 0;
  if (argv)
    while (argv[i])
      i++;
  arg_list = xcalloc (i+2, sizeof *arg_list);
  arg_list[0] = strrchr (pgmname, '/');
  if (arg_list[0])
    arg_list[0]++;
  else
    arg_list[0] = xstrdup (pgmname);
  if (argv)
    for (i=0,j=1; argv[i]; i++, j++)
      arg_list[j] = (char*)argv[i];

  /* Connect the standard files. */
  for (i = 0; i <= 2; i++)
    {
      if (fds[i] == -1)
        {
          fds[i] = open ("/dev/null", i ? O_WRONLY : O_RDONLY);
          if (fds[i] == -1)
            log_fatal ("failed to open `%s': %s\n",
                       "/dev/null", strerror (errno));
        }
      else if (fds[i] != i && dup2 (fds[i], i) == -1)
        log_fatal ("dup2 std%s failed: %s\n",
                   i==0?"in":i==1?"out":"err", strerror (errno));
    }

  /* Close all other files. */
  n = sysconf (_SC_OPEN_MAX);
  if (n < 0)
    n = MAX_OPEN_FDS;
  for (i=3; i < n; i++)
    close(i);
  errno = 0;
  
  execv (pgmname, arg_list);

  /* No way to print anything, as we have closed all streams. */
  _exit (127);
}
#endif /*!HAVE_W32_SYSTEM*/


/* Fork and exec the PGMNAME, connect the file descriptor of INFILE to
   stdin, write the output to OUTFILE, return a new stream in
   STATUSFILE for stderr and the pid of the process in PID. The
   arguments for the process are expected in the NULL terminated array
   ARGV.  The program name itself should not be included there.  if
   PREEXEC is not NULL, that function will be called right before the
   exec.

   Returns 0 on success or an error code. */
gpg_error_t
dirmngr_spawn_process (const char *pgmname, char *argv[],
		       int *fdout, int *fderr, pid_t *pid)
{
#ifdef HAVE_W32_SYSTEM
  gpg_error_t err;
  SECURITY_ATTRIBUTES sec_attr;
  PROCESS_INFORMATION pi = 
    {
      NULL,      /* Returns process handle.  */
      0,         /* Returns primary thread handle.  */
      0,         /* Returns pid.  */
      0          /* Returns tid.  */
    };
  STARTUPINFO si;
  int cr_flags;
  char *cmdline;
  int rp_stdout[2];
  int rp_stderr[2];
  HANDLE hnul = INVALID_HANDLE_VALUE;

  /* Setup return values.  */
  *fdout = -1;
  *fderr = -1;
  *pid = (pid_t)(-1);

  /* Build the command line.  */
  err = build_w32_commandline (pgmname, argv, &cmdline);
  if (err)
    return err; 

  /* Create a pipe.  */
  if (pth_pipe (rp_stdout, 1))
    {
      err = gpg_error (GPG_ERR_GENERAL);
      log_error (_("error creating a pipe: %s\n"), gpg_strerror (err));
      xfree (cmdline);
      return err;
    }

  if (pth_pipe (rp_stderr, 1))
    {
      err = gpg_error (GPG_ERR_GENERAL);
      log_error (_("error creating a pipe: %s\n"), gpg_strerror (err));
      xfree (cmdline);
      CloseHandle (fd_to_handle (rp_stdout[0]));
      CloseHandle (fd_to_handle (rp_stdout[1]));
      return err;
    }
  
  /* Prepare security attributes.  */
  memset (&sec_attr, 0, sizeof sec_attr);
  sec_attr.nLength = sizeof sec_attr;
  sec_attr.bInheritHandle = TRUE;
  hnul = CreateFile ("nul",
		     GENERIC_READ|GENERIC_WRITE,
		     FILE_SHARE_READ|FILE_SHARE_WRITE,
		     &sec_attr,
		     OPEN_EXISTING,
		     FILE_ATTRIBUTE_NORMAL,
		     NULL);
  if (hnul == INVALID_HANDLE_VALUE)
    {
      err = gpg_error (GPG_ERR_GENERAL);
      log_error (_("error opening nul: %s\n"), gpg_strerror (err));
      xfree (cmdline);
      CloseHandle (fd_to_handle (rp_stdout[0]));
      CloseHandle (fd_to_handle (rp_stdout[1]));
      CloseHandle (fd_to_handle (rp_stderr[0]));
      CloseHandle (fd_to_handle (rp_stderr[1]));
      return err;
    }
  
  /* Prepare security attributes.  */
  memset (&sec_attr, 0, sizeof sec_attr );
  sec_attr.nLength = sizeof sec_attr;
  sec_attr.bInheritHandle = FALSE;
  
  /* Start the process.  Note that we can't run the PREEXEC function
     because this would change our own environment. */
  memset (&si, 0, sizeof si);
  si.cb = sizeof (si);
  si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  si.wShowWindow = DEBUG_W32_SPAWN? SW_SHOW : SW_MINIMIZE;
  si.hStdInput  = hnul;
  si.hStdOutput = fd_to_handle (rp_stdout[1]);
  si.hStdError  = fd_to_handle (rp_stderr[1]);

  cr_flags = (CREATE_DEFAULT_ERROR_MODE
              | GetPriorityClass (GetCurrentProcess ())
              | CREATE_SUSPENDED); 
  log_debug ("CreateProcess, path=`%s' cmdline=`%s'\n", pgmname, cmdline);
  if (!CreateProcess (pgmname,       /* Program to start.  */
                      cmdline,       /* Command line arguments.  */
                      &sec_attr,     /* Process security attributes.  */
                      &sec_attr,     /* Thread security attributes.  */
                      TRUE,          /* Inherit handles.  */
                      cr_flags,      /* Creation flags.  */
                      NULL,          /* Environment.  */
                      NULL,          /* Use current drive/directory.  */
                      &si,           /* Startup information. */
                      &pi            /* Returns process information.  */
                      ))
    {
      log_error ("CreateProcess failed: %s\n", w32_strerror (-1));
      xfree (cmdline);
      CloseHandle (fd_to_handle (rp_stdout[0]));
      CloseHandle (fd_to_handle (rp_stdout[1]));
      CloseHandle (fd_to_handle (rp_stderr[0]));
      CloseHandle (fd_to_handle (rp_stderr[1]));
      CloseHandle (hnul);
      return gpg_error (GPG_ERR_GENERAL);
    }
  xfree (cmdline);
  cmdline = NULL;

  /* Close the other end of the pipe.  */
  CloseHandle (fd_to_handle (rp_stdout[1]));
  CloseHandle (fd_to_handle (rp_stderr[1]));
  CloseHandle (hnul);
  
  log_debug ("CreateProcess ready: hProcess=%p hThread=%p"
             " dwProcessID=%d dwThreadId=%d\n",
             pi.hProcess, pi.hThread,
             (int) pi.dwProcessId, (int) pi.dwThreadId);

  /* Process has been created suspended; resume it now. */
  ResumeThread (pi.hThread);
  CloseHandle (pi.hThread); 

  *fdout = handle_to_fd (rp_stdout[0]);
  *fderr = handle_to_fd (rp_stderr[0]);
  *pid = handle_to_pid (pi.hProcess);
  return 0;

#else /* !HAVE_W32_SYSTEM */
  gpg_error_t err;
  int rp_stdout[2];
  int rp_stderr[2];

  *fdout = -1;
  *fderr = -1;
  *pid = (pid_t)(-1);

  if (pipe (rp_stdout) == -1)
    {
      err = gpg_error_from_syserror ();
      log_error (_("error creating a pipe: %s\n"), strerror (errno));
      return err;
    }

  if (pipe (rp_stderr) == -1)
    {
      err = gpg_error_from_syserror ();
      log_error (_("error creating a pipe: %s\n"), strerror (errno));
      close (rp_stdout[0]);
      close (rp_stdout[1]);
      return err;
    }

  *pid = pth_fork ();
  if (*pid == (pid_t)(-1))
    {
      err = gpg_error_from_syserror ();
      log_error (_("error forking process: %s\n"), strerror (errno));
      close (rp_stdout[0]);
      close (rp_stdout[1]);
      close (rp_stderr[0]);
      close (rp_stderr[1]);
      return err;
    }

  if (!*pid)
    { 
      /* Run child. */
      do_exec (pgmname, argv, -1, rp_stdout[1], rp_stderr[1]);
      /*NOTREACHED*/
    }

  /* Parent. */
  close (rp_stdout[1]);
  close (rp_stderr[1]);

  *fdout = rp_stdout[0];
  *fderr = rp_stderr[0];

  return 0;
#endif /* !HAVE_W32_SYSTEM */
}


/* If HANG is true, waits for the process identified by PID to exit.
   If HANG is false, checks whether the process has terminated.
   Return values:

   GPG_ERR_NO_ERROR
       The process exited.  The exit code of process is then stored at
       R_STATUS.  An exit code of -1 indicates that the process
       terminated abnormally (e.g. due to a signal).

   GPG_ERR_TIMEOUT 
       The process is still running (returned only if HANG is false).

   GPG_ERR_INV_VALUE 
       An invalid PID has been specified.  

   Other error codes may be returned as well.  Unless otherwise noted,
   -1 will be stored at R_STATUS.  */      
gpg_error_t
dirmngr_wait_process (pid_t pid, int hang, int *r_status)
{
  gpg_err_code_t ec;

#ifdef HAVE_W32_SYSTEM
  HANDLE proc = pid_to_handle (pid);
  int code;
  DWORD exc;

  *r_status = -1;
  if (pid == (pid_t)(-1))
    return gpg_error (GPG_ERR_INV_VALUE);

  /* FIXME: We should do a pth_waitpid here.  However this has not yet
     been implemented.  A special W32 pth system call would even be
     better.  */
  code = WaitForSingleObject (proc, hang ? INFINITE : 0);
  switch (code) 
    {
    case WAIT_TIMEOUT:
      ec = GPG_ERR_TIMEOUT;
      break;

    case WAIT_FAILED:
      log_error (_("waiting for process %d to terminate failed: %s\n"),
		 (int)pid, w32_strerror (-1));
      ec = 0;
      break;
      
    case WAIT_OBJECT_0:
      if (!GetExitCodeProcess (proc, &exc))
	{
	  log_error (_("error getting exit code of process %d: %s\n"),
		     (int)pid, w32_strerror (-1) );
	  ec = GPG_ERR_GENERAL;
	}
      else 
	{
          *r_status = exc;
          if (exc)
            log_error (_("error detected: exit status %d%s\n"), 
                       *r_status, "");
          ec = 0;
	}
      break;
      
    default:
      log_error ("WaitForSingleObject returned unexpected "
		 "code %d for pid %d\n", code, (int)pid );
      ec = GPG_ERR_GENERAL;
      break;
    }
  
#else /* !HAVE_W32_SYSTEM */
  int i;
  int status;

  *r_status = -1;

  if (pid == (pid_t)(-1))
    return gpg_error (GPG_ERR_INV_VALUE);

  i = pth_waitpid (pid, &status, hang? 0 : WNOHANG);
  if (i == (pid_t)(-1))
    {
      ec = gpg_err_code_from_syserror ();
      log_error (_("waiting for process %d to terminate failed: %s\n"),
                 (int)pid, strerror (errno));
    }
  else if (i == 0)
    {
      /* The process is still running.  */
      ec = GPG_ERR_TIMEOUT;
    }
  else if (WIFEXITED (status))
    {
      ec = 0;
      *r_status = WEXITSTATUS (status);
      if (*r_status)
        log_error (_("error detected: exit status %d%s\n"), *r_status,
                   *r_status == 127? _(" (program probably not installed)")
                   /* */              :"");
    }
  else
    {
      ec = 0;
      log_error (_("error detected: terminated\n"));
    }
#endif /* !HAVE_W32_SYSTEM */

  return gpg_err_make (GPG_ERR_SOURCE_DEFAULT, ec);

}


/* Kill the program PID with name PGMNAME (only used for
   diagnostics).  */
gpg_error_t
dirmngr_kill_process (pid_t pid)
{
#ifdef HAVE_W32_SYSTEM
  /* FIXME: Implement something.  TerminateProcess may compromise the
     state of global data held by DLLs, but seems our best (or only?)
     shot.  */
  return 0;
#else
  return kill (pid, SIGTERM);
#endif
}


gpg_error_t
dirmngr_release_process (pid_t pid)
{
#ifdef HAVE_W32_SYSTEM
  CloseHandle (pid_to_handle (pid));
#else
  (void)pid;
#endif
  return 0;
}
