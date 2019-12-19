#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint32_t);

/* We use the ``readline'' library to provide more flexibility to read from stdin. */
char* rl_gets() {
	static char *line_read = NULL;

	if (line_read) {
		free(line_read);
		line_read = NULL;
	}

	line_read = readline("(nemu) ");

	if (line_read && *line_read) {
		add_history(line_read);
	}

	return line_read;
}

static int cmd_c(char *args) {
	cpu_exec(-1);
	return 0;
}

static int cmd_q(char *args) {
	return -1;
}

static int cmd_help(char *args);

#define MAX_INSTR_TO_PRINT 10
//需手动与cpu-exec.c中的定义保持一致
static int cmd_si(char* args) {
	int n = args == NULL ? 1 : atoi(args);
	if (n == 0) {printf("未知参数'%s'\n", args); return 1;} //输入的参数不能被atoi()识别
	cpu_exec(n);
	if (n >= MAX_INSTR_TO_PRINT) printf("执行完毕，由于系统设定%d条以上指令不会被打印，故不予显示。\n", MAX_INSTR_TO_PRINT);
	return 0;
}

static int cmd_info(char* args) {
	if (args == NULL) {printf("您需要什么信息？(r/w)\n"); return 1;}
	else if (args[0] == 'r')
	{
		for (int i = R_EAX; i <= R_EDI; i++)
			printf("%s 0x%08x %-10d\n", regsl[i], reg_l(i), reg_l(i));
		printf("eip 0x%08x %-10d\n", cpu.eip, cpu.eip);
	}
	else if (args[0] == 'w') print_wp();
	else printf("未知参数'%s'\n", args);
	return 0;
}

static int cmd_x(char* args) {
	int addr, num;
	char exp[128];
	if (args == NULL) {printf("您需要查看哪些内存？(x [num] [addr])\n"); return 1;}
	if (sscanf(args, "%d%[^\n]", &num, exp) < 2) {printf("未知参数'%s'\n", args); return 1;}
	bool suc = 1;
	addr = expr(exp, &suc);
	if (!suc) return 1; //处理表达式时会输出错误信息，所以外层调用处不再输出
	printf("0x%08x:\t", addr);
	for (int i = 0; i < num; i++) printf(" 0x%08x", swaddr_read(addr+4*i, 4));
	printf("\n");
	return 0;
}

static int cmd_p(char* arg)
{
	bool eval = 1;
	int val = expr(arg, &eval);
	if (eval == 0) return 1; //把输出提示信息放在求值过程
	printf("%d\n", val);
	return 0;
}
static int cmd_w(char* arg)
{
	WP* wp = new_wp(); //若申请失败会在申请时输出错误信息
	if (wp == NULL) return 3;
	strcpy(wp -> exp, arg);
	bool success = 1;
	wp -> val = expr(arg, &success);
	if (!success) {free_wp(wp); return 1;} //会在处理表达式时输出错误信息
	printf("监视点%d: %s\n", wp -> NO, wp -> exp);
	return 0;
}
static int cmd_b(char* arg)
{
	if (arg == NULL) {printf("您想在什么地址设置断点？\n提示：断点也可通过在用户程序中直接调用set_bp()函数来设定。\n"); return 3;}
	char* n_arg = malloc(strlen(arg) + 8);
	strcpy(n_arg, "$eip == "); strcat(n_arg, arg);
	int r = cmd_w(n_arg);
	puts("提示：断点也可通过在用户程序中直接调用set_bp()函数来设定。");
	free(n_arg);
	return r;
}
static int cmd_d(char* arg)
{
	int no = atoi(arg);
	return free_wp_no(no);
}
struct PartOfStackFrame
{
	swaddr_t prev_ebp;
	swaddr_t ret_addr;
	uint32_t args[5];
};
//符号表
#include <elf.h>
extern char* strtab;
extern Elf32_Sym *symtab;
extern int nr_symtab_entry;
static int cmd_bt()
{
	//打印地址、函数名，然后顺序往高地址读5个参数(也可能是上一个栈帧的局部变量，但讲义要求强行打印出来)
	uint32_t cur_ebp = cpu.ebp, cur_eip = cpu.eip, prev_ebp, prev_eip;
	int dep = 0;
	while (cur_ebp != 0)
	{
		printf("#%d\t", dep);
		printf("0x%x in ", cur_eip);
		int i = 0;
		for (i = 0; i < nr_symtab_entry; i++)
			if (ELF32_ST_TYPE(symtab[i].st_info) == STT_FUNC && symtab[i].st_value <= cur_eip && cur_eip < symtab[i].st_value + symtab[i].st_size)
			{
				printf("%s ", strtab + symtab[i].st_name);
				break;
			}
		if (i == nr_symtab_entry) {puts("unknown function (符号表中size项为0的函数，无法识别)"); break;}

		prev_ebp = swaddr_read(cur_ebp, 4); cur_ebp += 4;
		prev_eip = swaddr_read(cur_ebp, 4); cur_ebp += 4;
		printf("(");
		for (int i = 0; i < 5 && cur_ebp < 0x8000000; i++)
		{
			printf("%d, ", swaddr_read(cur_ebp, 4));
			cur_ebp += 4;
		} //因为栈内总共的元素个数可能都不足5个，所以要加上越界判断
		printf("...)\n");

		cur_ebp = prev_ebp; cur_eip = prev_eip; dep++;
	}
	return 0;
}

static struct {
	char *name;
	char *description;
	int (*handler) (char *);
} cmd_table [] = {
	{ "help", "Display informations about all supported commands", cmd_help },
	{ "c", "Continue the execution of the program", cmd_c },
	{ "q", "Exit NEMU", cmd_q },
	{ "si", "单步执行", cmd_si },
	{ "info", "打印寄存器或监视点信息", cmd_info },
	{ "x", "扫描内存", cmd_x },
	{ "p", "表达式求值", cmd_p },
	{ "w", "监视点", cmd_w },
	{ "d", "删除监视点", cmd_d },
	{ "bt", "打印栈帧链", cmd_bt},
	{ "b", "设置断点", cmd_b},
	/* TODO: Add more commands */

};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
	/* extract the first argument */
	char *arg = strtok(NULL, " ");
	int i;

	if(arg == NULL) {
		/* no argument given */
		for(i = 0; i < NR_CMD; i ++) {
			printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
		}
	}
	else {
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(arg, cmd_table[i].name) == 0) {
				printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
				return 0;
			}
		}
		printf("Unknown command '%s'\n", arg);
	}
	return 0;
}

void ui_mainloop() {
	while(1) {
		char *str = rl_gets();
		char *str_end = str + strlen(str);

		/* extract the first token as the command */
		char *cmd = strtok(str, " ");
		if(cmd == NULL) { continue; }

		/* treat the remaining string as the arguments,
		 * which may need further parsing
		 */
		char *args = cmd + strlen(cmd) + 1;
		if(args >= str_end) {
			args = NULL;
		}

#ifdef HAS_DEVICE
		extern void sdl_clear_event_queue(void);
		sdl_clear_event_queue();
#endif

		int i;
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(cmd, cmd_table[i].name) == 0) {
				if(cmd_table[i].handler(args) < 0) { return; }
				break;
			}
		}

		if(i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
	}
}
