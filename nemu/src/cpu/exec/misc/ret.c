#include "cpu/exec/helper.h"

make_helper(ret_near)
{
	cpu.eip = swaddr_read(cpu.esp, 4);
	cpu.esp += 4;
	
	print_asm("ret");
	return 0;
}

