#include "cpu/exec/template-start.h"

#define instr call

make_helper(concat(call_rel_, SUFFIX))
{
	int len = concat(decode_i_, SUFFIX)(eip + 1);
#if DATA_BYTE == 2
	uint32_t oeip = (cpu.eip & 0x0000ffff) + 3; //cpu.IP
	swaddr_t neip = (cpu.eip + op_src->val) & 0x0000ffff;
#else
	uint32_t oeip = cpu.eip + 5;
	swaddr_t neip = cpu.eip + op_src->val;
#endif
	cpu.esp -= DATA_BYTE;
	MEM_W(cpu.esp, oeip);
	cpu.eip = neip;
	//no EFLAGS affected since no task switch occurs
	
	print_asm_template1();
	return 1 + len;
}

#include "cpu/exec/template-end.h"
