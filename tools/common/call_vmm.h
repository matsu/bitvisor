#include <stdint.h>
#define CALL_VMM_GET_FUNCTION_LINE_SUB(name, ret, line) do { \
	intptr_t call_vmm__tmp; \
	extern char call_vmm__##line##_asm0[] \
		asm ("CALL_VMM_GET_FUNCTION_" #line "_0"); \
	extern char call_vmm__##line##_asm1[] \
		asm ("CALL_VMM_GET_FUNCTION_" #line "_1"); \
 \
	asm volatile ("jmp 2f\n" \
		      "CALL_VMM_GET_FUNCTION_" #line "_0:\n" \
		      "0: vmcall; jmp *%0; .org 0b+5; vmmcall; jmp *%0;" \
		      "   .org 0b+10; .string \"" name "\"\n" \
		      "CALL_VMM_GET_FUNCTION_" #line "_1:\n" \
		      "1: vmcall; jmp *%0; .org 1b+5; vmmcall; jmp *%0;" \
		      "   .org 1b+10; .string \"" name "\"\n" \
		      "2:" \
		      : "=S" (call_vmm__tmp)); \
	call_vmm_get_function ((intptr_t)call_vmm__##line##_asm0, \
			       (intptr_t)call_vmm__##line##_asm1, \
			       5, 10, (ret)); \
} while (0)
#define CALL_VMM_GET_FUNCTION_LINE(name, ret, line) \
	CALL_VMM_GET_FUNCTION_LINE_SUB (name, ret, line)
/* Using this macro multiple times in a line causes errors */
#define CALL_VMM_GET_FUNCTION(name, ret) \
	CALL_VMM_GET_FUNCTION_LINE (name, ret, __LINE__)

typedef struct {
	int vmmcall_number;
	int vmmcall_type;
} call_vmm_function_t;

typedef struct {
	intptr_t rbx, rcx, rdx, rsi, rdi;
} call_vmm_arg_t;

typedef struct {
	intptr_t rax, rbx, rcx, rdx, rsi, rdi;
} call_vmm_ret_t;

static inline int
call_vmm_function_no_vmm (call_vmm_function_t *f)
{
	return !f->vmmcall_type;
}

static inline int
call_vmm_function_callable (call_vmm_function_t *f)
{
	return f->vmmcall_type && f->vmmcall_number;
}

int call_vmm_docall (void (*func) (void *data), void *data);
void call_vmm_get_function (intptr_t addr0, intptr_t addr1,
			    int aoff, int off, call_vmm_function_t *function);
void call_vmm_call_function (call_vmm_function_t *function,
			     call_vmm_arg_t *arg, call_vmm_ret_t *ret);
