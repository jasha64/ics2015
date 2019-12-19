#include "cpu/exec/template-start.h"

#define instr push

static void do_execute() {
	//我们已知地址位数为32
	cpu.esp -= DATA_BYTE;
	MEM_W(cpu.esp, op_src->val);
	//no EFLAGS affected
	print_asm_template1();
}

make_instr_helper(r)

#include "cpu/exec/template-end.h"
