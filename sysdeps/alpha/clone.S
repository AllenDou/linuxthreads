/* Copyright (C) 1996, 1997 Free Software Foundation, Inc.
   Contributed by Richard Henderson (rth@tamu.edu)

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
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

/* clone() is even more special than fork() as it mucks with stacks
   and invokes a function in the right context after its all over.  */

#include <alpha/regdef.h>
#include <asm/pal.h>
#include <asm/errno.h>
#include <asm/unistd.h>

/* int clone(int (*fn)(void *arg), void *child_stack, int flags, void *arg) */

        .text

	.align 3
	.ent __clone
	.globl __clone
	.weak	clone
	clone   = __clone
__clone:
        lda     sp,-16(sp)
        .frame  sp,16,$26,0
	.prologue 1

	/* Sanity check arguments.  */
	ldiq	v0,EINVAL
	beq	a0,$error		/* no NULL function pointers */
	beq	a1,$error		/* no NULL stack pointers */

	/* Do the system call */
	mov	a0,pv			/* get fn ptr out of the way */
	mov	a3,a5			/* get fn arg out of the way */
	mov	a2,a0
	ldiq	v0,__NR_clone
	call_pal PAL_callsys

	bne	a3,$error
	beq	v0,thread_start

	/* Successful return from the parent */
	lda	sp,16(sp)
	ret

	/* Something bad happened -- no child created */
$error:
	br	gp,1f
1:	ldgp	gp,0(gp)
        stl     v0,0(sp)
        jsr     __errno_location
        ldl     a0,0(sp)
        stl     a0,0(v0)
	ldiq	v0,-1
	lda	sp,16(sp)
	ret

	.end __clone

/* Load up the arguments to the function.  Put this block of code in
   its own function so that we can terminate the stack trace with our
   debug info. */

	.ent thread_start
thread_start:
	.frame fp,0,zero,0
	mov	zero,fp
	.prologue 0

	/* Call the user's function */
	mov	a5,a0
	jsr	ra,(pv)
	ldgp	gp,0(ra)

	/* Call _exit rather than doing it inline for breakpoint purposes */
	mov	v0,a0
	jsr	ra,_exit

	/* Die horribly */
	call_pal PAL_halt

	.end thread_start
