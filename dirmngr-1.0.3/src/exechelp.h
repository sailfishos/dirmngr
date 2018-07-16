/* exechelp.h - fork and exec helpers
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

#ifndef DIRMNGR_EXECHELP_H
#define DIRMNGR_EXECHELP_H

/* Fork and exec the PGMNAME with /dev/null as stdin, and pipes for
   stdout and stderr.  The read ends of these pipes are returned in
   FDOUT and FDERR, as well as the pid of the process in PID. The
   arguments for the process are expected in the NULL terminated array
   ARGV.  The program name itself should not be included there.
   Returns 0 on success or an error code.  */
gpg_error_t dirmngr_spawn_process (const char *pgmname, char *argv[],
				   int *fdout, int *fderr, pid_t *pid);

/* Wait for the process identified by PID to terminate.  Returns -1 if
   an error occurs, if the process succeded, GPG_ERR_GENERAL for any
   failures of the spawned program or other error codes.  STATUS is
   set to 0 if timeout occurs.  */
gpg_error_t dirmngr_wait_process (pid_t pid, int hang, int *status);

/* Kill the program PID.  */
gpg_error_t dirmngr_kill_process (pid_t pid);

/* Release the PID.  */
gpg_error_t dirmngr_release_process (pid_t pid);

#endif /* DIRMNGR_EXECHELP_H */
