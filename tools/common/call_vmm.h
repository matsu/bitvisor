#define CALL_VMM_GET_FUNCTION(name, ret) do { \
	unsigned long call_vmm__addr0, call_vmm__addr1; \
 \
	asm volatile ("jmp 2f\n" \
		      "0: vmcall; jmp *%1; .org 0b+5; vmmcall; jmp *%1;" \
		      "   .org 0b+10; .string \"" name "\"\n" \
		      "1: vmcall; jmp *%1; .org 1b+5; vmmcall; jmp *%1;" \
		      "   .org 1b+10; .string \"" name "\"\n" \
		      "2: mov $0b,%0; mov $1b,%1" \
		      : "=r" (call_vmm__addr0), "=S" (call_vmm__addr1)); \
	call_vmm_get_function (call_vmm__addr0, call_vmm__addr1, \
			       5, 10, (ret)); \
} while (0)

typedef struct {
	int vmmcall_number;
	int vmmcall_type;
} call_vmm_function_t;

typedef struct {
	long rbx, rcx, rdx, rsi, rdi;
} call_vmm_arg_t;

typedef struct {
	long rax, rbx, rcx, rdx, rsi, rdi;
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
void call_vmm_get_function (unsigned long addr0, unsigned long addr1,
			    int aoff, int off, call_vmm_function_t *function);
void call_vmm_call_function (call_vmm_function_t *function,
			     call_vmm_arg_t *arg, call_vmm_ret_t *ret);
