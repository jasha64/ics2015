#include "cpu/exec/template-start.h"

#define instr jcc

static void do_execute() {
	cpu.eip += op_src->val;
#if DATA_BYTE == 2
	cpu.eip &= 0x0000ffff;
#endif
	//no EFLAGS affected
}

make_helper(concat(jcc_zf_, SUFFIX))
{
	int len = concat(decode_si_, SUFFIX)(eip + 1);
	if (cpu.ZF) do_execute();

	print_asm("jzf %s", op_src->str);
	return 1 + len;
}

#if DATA_BYTE == 1
make_helper(jcc_cf_zf_b)
{
	int len = decode_si_b(eip + 1);
	if (cpu.CF || cpu.ZF) do_execute();

	print_asm("jcfzf %s", op_src->str);
	return 1 + len;
}
#endif

#include "cpu/exec/template-end.h"
