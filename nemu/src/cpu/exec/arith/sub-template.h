#include "cpu/exec/template-start.h"

#define UPD_CF(a, b) cpu.CF = a < b
#define UPD_OF(a, b) cpu.OF = ((a) >> (8*DATA_BYTE-1)) == ((-b) >> (8*DATA_BYTE-1)) && ((a) >> (8*DATA_BYTE-1)) != (result >> (8*DATA_BYTE-1))
#define instr sub

static void do_execute() {
	uint32_t a = op_dest->val, b = op_src->val;
	DATA_TYPE result = a - b;
	OPERAND_W(op_dest, result);
	UPD_CF(a, b); UPD_PF(); UPD_ZF(); UPD_SF(); UPD_OF(a, b);
	print_asm_template2();
}

make_instr_helper(i2a)
make_instr_helper(i2rm)
#if DATA_BYTE == 2 || DATA_BYTE == 4
make_instr_helper(si2rm)
#endif
make_instr_helper(r2rm)
make_instr_helper(rm2r)

#undef UPD_CF
#undef UPD_OF
#include "cpu/exec/template-end.h"
