#include "cpu/exec/template-start.h"

#define instr jmp

make_helper(concat(jmp_rel_, SUFFIX))
{
	int len = concat(decode_i_, SUFFIX)(eip + 1);
	cpu.eip += op_src->val;
#if DATA_BYTE == 2
	cpu.eip &= 0x0000ffff;
#endif
	//no EFLAGS affected since no task switch occurs
	
	print_asm_template1();
	return 1 + len;
}

#include "cpu/exec/template-end.h"
