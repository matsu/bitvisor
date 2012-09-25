#define CALL_VMM_GET_FUNCTION(name, ret) do { \
	unsigned long call_vmm__addr0, call_vmm__addr1; \
	unsigned long call_vmm__addr2, call_vmm__addr3; \
 \
	asm volatile ("jmp 4f\n" \
		      "0: vmcall; ret; .org 0b+4; .string \"" name "\"\n" \
		      "1: vmcall; ret; .org 1b+4; .string \"" name "\"\n" \
		      "2: vmmcall; ret; .org 2b+4; .string \"" name "\"\n" \
		      "3: vmmcall; ret; .org 3b+4; .string \"" name "\"\n" \
		      "4: mov $0b,%0; mov $1b,%1; mov $2b,%2; mov $3b,%3" \
		      : "=r" (call_vmm__addr0), "=r" (call_vmm__addr1) \
		      , "=r" (call_vmm__addr2), "=r" (call_vmm__addr3)); \
	call_vmm_get_function (call_vmm__addr0, call_vmm__addr1, \
			       call_vmm__addr2, call_vmm__addr3, 4, (ret)); \
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
			    unsigned long addr2, unsigned long addr3,
			    int off, call_vmm_function_t *function);
void call_vmm_call_function (call_vmm_function_t *function,
			     call_vmm_arg_t *arg, call_vmm_ret_t *ret);
