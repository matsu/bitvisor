#define _BSD_SOURCE
#define _DEFAULT_SOURCE
/* _BSD_SOURCE for glibc version < 2.19 */
/* _DEFAULT_SOURCE suppresses a warning of glibc version >= 2.20 */

#include <setjmp.h>
#include <signal.h>
#include "call_vmm.h"

#define call_vmm_jmp_buf jmp_buf
#define call_vmm_setjmp(env) setjmp (env)
#define call_vmm_longjmp(env, val) longjmp (env, val)
#ifdef __GLIBC__
#ifdef __GLIBC_MINOR__
#if __GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 19)
/* glibc version >= 2.19 does not provide BSD compatible setjmp.
   sigsetjmp should be used but it is not defined in MinGW.  These
   macros replace them for glibc version >= 2.19. */
#undef call_vmm_jmp_buf
#undef call_vmm_setjmp
#undef call_vmm_longjmp
#define call_vmm_jmp_buf sigjmp_buf
#define call_vmm_setjmp(env) sigsetjmp (env, 1)
#define call_vmm_longjmp(env, val) siglongjmp (env, val)
#endif
#endif
#endif

#define VMMCALL_TYPE_ERROR 0
#define VMMCALL_TYPE_VMCALL 1
#define VMMCALL_TYPE_VMMCALL 2

struct call_vmm_get_function_sub_data {
	intptr_t addr, arg;
	int ret;
};

struct call_vmm_call_function_sub_data {
	call_vmm_function_t *function;
	call_vmm_arg_t *arg;
	call_vmm_ret_t *ret;
};

static call_vmm_jmp_buf jmpbuf;

static void
call_vmm_signalhandler (int sig)
{
	call_vmm_longjmp (jmpbuf, 1);
}

static volatile int
call_vmm_docall_sub (void (*func) (void *data), void *data)
{
	if (!call_vmm_setjmp (jmpbuf)) {
		func (data);
		return 0;
	} else {
		return 1;
	}
}

int
call_vmm_docall (void (*func) (void *data), void *data)
{
	void (*old_sigill) (int), (*old_sigsegv) (int);
	int ret;

	old_sigill = signal (SIGILL, call_vmm_signalhandler);
	old_sigsegv = signal (SIGSEGV, call_vmm_signalhandler);
	ret = call_vmm_docall_sub (func, data);
	signal (SIGILL, old_sigill);
	signal (SIGSEGV, old_sigsegv);
	return ret;
}

static void
call_vmm_get_function_sub (void *data)
{
	struct call_vmm_get_function_sub_data *p;
	extern char done[] asm ("call_vmm_get_function_sub__done");

	p = data;
	asm volatile ("jmp *%3\n"
		      "call_vmm_get_function_sub__done:"
		      : "=a" (p->ret)
		      : "a" (0), "b" (p->arg), "r" (p->addr), "S" (done));
}

void
call_vmm_get_function (intptr_t addr0, intptr_t addr1, int aoff,
		       int off, call_vmm_function_t *function)
{
	struct call_vmm_get_function_sub_data data;

	if ((addr0 ^ addr1) < 0x1000)
		data.addr = addr0;
	else
		data.addr = addr1;
	data.arg = data.addr + off;
	if (!call_vmm_docall (call_vmm_get_function_sub, &data)) {
		function->vmmcall_number = data.ret;
		function->vmmcall_type = VMMCALL_TYPE_VMCALL;
		return;
	}
	data.addr += aoff;
	if (!call_vmm_docall (call_vmm_get_function_sub, &data)) {
		function->vmmcall_number = data.ret;
		function->vmmcall_type = VMMCALL_TYPE_VMMCALL;
		return;
	}
	function->vmmcall_type = VMMCALL_TYPE_ERROR;
}

static void
call_vmm_call_function_sub (void *data)
{
	struct call_vmm_call_function_sub_data *p;

	p = data;
	switch (p->function->vmmcall_type) {
	case VMMCALL_TYPE_VMCALL:
		asm volatile ("vmcall"
			      : "=a" (p->ret->rax), "=b" (p->ret->rbx),
				"=c" (p->ret->rcx), "=d" (p->ret->rdx),
				"=S" (p->ret->rsi), "=D" (p->ret->rdi)
			      : "a" (p->function->vmmcall_number),
				"b" (p->arg->rbx),
				"c" (p->arg->rcx), "d" (p->arg->rdx),
				"S" (p->arg->rsi), "D" (p->arg->rdi)
			      : "memory");
		break;
	case VMMCALL_TYPE_VMMCALL:
		asm volatile ("vmmcall"
			      : "=a" (p->ret->rax), "=b" (p->ret->rbx),
				"=c" (p->ret->rcx), "=d" (p->ret->rdx),
				"=S" (p->ret->rsi), "=D" (p->ret->rdi)
			      : "a" (p->function->vmmcall_number),
				"b" (p->arg->rbx),
				"c" (p->arg->rcx), "d" (p->arg->rdx),
				"S" (p->arg->rsi), "D" (p->arg->rdi)
			      : "memory");
		break;
	}
}

void
call_vmm_call_function (call_vmm_function_t *function,
			call_vmm_arg_t *arg, call_vmm_ret_t *ret)
{
	struct call_vmm_call_function_sub_data data;

	data.function = function;
	data.arg = arg;
	data.ret = ret;
	call_vmm_docall (call_vmm_call_function_sub, &data);
}
