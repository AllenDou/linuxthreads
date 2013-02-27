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

#include <asm/errno.h>
#include <asm/unistd.h>

/* int clone(int (*fn)(void *arg), void *child_stack, int flags, void *arg); */

        .text
	.align	4
	.globl	__clone
        .type   __clone,@function
	.weak	clone
	clone   = __clone
__clone:
	/* Sanity check arguments.  */
	movl	$-EINVAL,%eax
	movl	4(%esp),%ecx		/* no NULL function pointers */
	testl	%ecx,%ecx
	jz	syscall_error
	movl	8(%esp),%ecx		/* no NULL stack pointers */
	testl	%ecx,%ecx
	jz	syscall_error

	/* Insert the argument onto the new stack.  */
	subl	$8,%ecx
	movl	16(%esp),%eax
	movl	%eax,4(%ecx)

	/* Save the function pointer as the zeroth argument.
	   It will be popped off in the child in the ebx frobbing below.  */
	movl	4(%esp),%eax
	movl	%eax,0(%ecx)

	/* Do the system call */
	pushl	%ebx
	movl	16(%esp),%ebx
	movl	$__NR_clone,%eax
	int	$0x80
	popl	%ebx

	test	%eax,%eax
	jl	syscall_error
	jz	thread_start

	ret

syscall_error:
#ifdef __PIC__
        pushl   %ebx
        call    syscall_error_1
syscall_error_1:
        popl    %ebx
        addl    $_GLOBAL_OFFSET_TABLE_+[.-syscall_error_1], %ebx	
#endif
	negl    %eax
        pushl   %eax
#ifdef __PIC__
        call    __errno_location@PLT
#else
        call    __errno_location
#endif
        popl    0(%eax)
	movl	$-1, %eax
#ifdef __PIC
        popl    %ebx
#endif
	ret

thread_start:
	subl	%ebp,%ebp	/* terminate the stack frame */
	call	*%ebx
        pushl   %eax
#ifdef __PIC__
        call    thread_exit
thread_exit:
        popl    %ebx
        addl    $_GLOBAL_OFFSET_TABLE_+[.-thread_exit], %ebx	
	call	_exit@PLT
#else
	call	_exit
#endif

