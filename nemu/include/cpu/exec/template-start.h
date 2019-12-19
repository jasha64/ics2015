#include "cpu/exec/helper.h"

#if DATA_BYTE == 1

#define SUFFIX b
#define DATA_TYPE uint8_t
#define DATA_TYPE_S int8_t

#elif DATA_BYTE == 2

#define SUFFIX w
#define DATA_TYPE uint16_t
#define DATA_TYPE_S int16_t

#elif DATA_BYTE == 4

#define SUFFIX l
#define DATA_TYPE uint32_t
#define DATA_TYPE_S int32_t

#else

#error unknown DATA_BYTE

#endif

#define REG(index) concat(reg_, SUFFIX) (index)
#define REG_NAME(index) concat(regs, SUFFIX) [index]

#define MEM_R(addr) swaddr_read(addr, DATA_BYTE)
#define MEM_W(addr, data) swaddr_write(addr, DATA_BYTE, data)

#define OPERAND_W(op, src) concat(write_operand_, SUFFIX) (op, src)

#define MSB(n) ((DATA_TYPE)(n) >> ((DATA_BYTE << 3) - 1))

#define UPD_PF() cpu.PF = ((result & 1) + (result >> 1 & 1) + (result >> 2 & 1) + (result >> 3 & 1) + (result >> 4 & 1) + (result >> 5 & 1) + (result >> 6 & 1) + (result >> 7 & 1)) % 2 == 0 
#define UPD_ZF() cpu.ZF = result == 0
#define UPD_SF() cpu.SF = result >> (8 * DATA_BYTE - 1) & 1
#define CLR_OF() cpu.OF = 0
#define CLR_CF() cpu.CF = 0
