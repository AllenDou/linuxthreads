/* Machine-dependent pthreads configuration and inline functions.
   PowerPC version. */

/* Linuxthreads - a simple clone()-based implementation of Posix        */
/* threads for Linux.                                                   */
/* Copyright (C) 1997 Xavier Leroy (Xavier.Leroy@inria.fr)              */
/*                                                                      */
/* This program is free software; you can redistribute it and/or        */
/* modify it under the terms of the GNU Library General Public License  */
/* as published by the Free Software Foundation; either version 2       */
/* of the License, or (at your option) any later version.               */
/*                                                                      */
/* This program is distributed in the hope that it will be useful,      */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of       */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        */
/* GNU Library General Public License for more details.                 */

/* Spinlock implementation; required.  */
static inline int testandset(int *spinlock)
{
  int ret;

  __asm__ __volatile__(
      "1:\t"
      "lwarx %0, 0, %2\n\t"
      "stwcx. %3, 0, %2\n\t"
      "bne 1b\n"
      "2:"
      : "=&r" (ret), "=m" (*spinlock)
      : "r" (spinlock), "r" (1)
      : "cr0");
  return ret;
}

/* Get some notion of the current stack.  Need not be exactly the top
   of the stack, just something somewhere in the current frame.  */
#define CURRENT_STACK_FRAME  stack_pointer
register char *stack_pointer __asm__("1");

/* Compare-and-swap for semaphores. */

#define HAS_COMPARE_AND_SWAP

static inline int compare_and_swap(long * p, long oldval, long newval)
{
  int ret;
  long temp;

  __asm__ __volatile__ (
      "lwarx %1, 0, %3\n\t"
      "cmp %1, %4\n\t"
      "li %0, 0\n\t"
      "bne 1f\n\t"
      "stwcx. %5, 0, %3\n\t"
      "bne 1f\n\t"
      "li %0, 1\n"
      "1:"
   : "=&r" (ret), "=&r" (temp), "=m" (*p)
   : "r" (p), "r" (oldval), "r" (newval)
   : "cr0");
  return ret;
}
