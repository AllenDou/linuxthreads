/* Copyright (C) 1991, 1992 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the, 1992 Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

/*
 *     POSIX Standard: 5.1.2 Directory Operations      <dirent.h>
 */

#ifndef        _DIRSTREAM_H

#define        _DIRSTREAM_H    1

#include <dirent.h>
#ifdef _POSIX_THREADS
#include "../pthread.h"
#endif

/* For now, syscall readdir () only supports one entry at a time. It
 * will be changed in the future.
#define NUMENT         3
*/
#ifndef NUMENT
#define NUMENT         1
#endif

#define SINGLE_READDIR 11
#define MULTI_READDIR  12
#define NEW_READDIR    13

/* Directory stream type.  */
struct DIR {
  /* file descriptor */
  int dd_fd;

  /* offset of the next dir entry in buffer */
  off_t dd_nextloc;

  /* bytes of valid entries in buffer */
  size_t dd_size;

  /* -> directory buffer */
  struct dirent *dd_buf;

  /* offset of the next dir entry in directory. */
  off_t dd_nextoff;

  /* total size of buffer */
  size_t dd_max;

  enum {unknown, have_getdents, no_getdents} dd_getdents;

  /* lock */
#ifdef _POSIX_THREADS
  pthread_mutex_t *dd_lock;
#else
  void *dd_lock;
#endif
};                             /* stream data from opendir() */

#ifdef __ELF__
extern DIR *__libc_opendir __P ((__const char *__name));
extern int __libc_closedir __P ((DIR * __dirp));
extern struct dirent *__libc_readdir __P ((DIR * __dirp));
extern void __libc_rewinddir __P ((DIR * __dirp));
extern void __libc_seekdir __P ((DIR * __dirp, __off_t __pos));
extern __off_t __libc_telldir __P ((DIR * __dirp));
extern int __libc_readdir_r __P((DIR *__dirp, struct dirent *__entry,
       struct dirent **__result));
#else
#define  __libc_opendir opendir
#define  __libc_closedir closedir
#define  __libc_rewinddir rewinddir
#define  __libc_seekdir seekdir
#define  __libc_telldir telldir
#define  __libc_readdir readdir
#define  __libc_readdir_r readdir_r
#endif

#endif /* dirent.h  */
