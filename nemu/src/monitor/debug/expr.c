#include "nemu.h"
#include <stdlib.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>

//for variables
#include <elf.h>

enum {
	NOTYPE = 256, DECNUM = '0', HEXNUM = 'x', REG = 'r',
	NON_EQ = 1, NOT = 2, DEREF = 3, NEGATE = 4, VAR = 'v'
};

extern char *strtab;
extern Elf32_Sym *symtab;
extern int nr_symtab_entry;

static struct rule {
	char *regex;
	int token_type;
} rules[] = {
	// Pay attention to the precedence level of different rules.
	{" +",	NOTYPE}, // spaces
	{"\\$(eax|ecx|edx|ebx|esp|ebp|esi|edi|eip|ax|cx|dx|bx|sp|bp|si|di|al|cl|dl|bl|ah|ch|dh|bh)", REG}, //寄存器
	{"0x[0-9,a-f,A-F]+", HEXNUM}, //十六进制数
	{"[0-9]+", DECNUM}, //十进制数
	{"\\*", '*'}, //乘
	{"/", '/'}, //除
	{"\\+", '+'}, // plus
	{"\\-", '-'}, //减
	{"\\(", '('}, //左括号
	{"\\)", ')'}, //右括号
	{"==", '='}, // equal
	{"!=", NON_EQ}, //不等于
	{"&&", '&'}, //与
	{"\\|\\|", '|'}, //或
	{"!", NOT}, //非
	{"[a-z,A-Z,_][a-z,A-Z,0-9,_]*", VAR} //变量
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
	int i;
	char error_msg[128];
	int ret;

	for(i = 0; i < NR_REGEX; i ++) {
		ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
		if(ret != 0) {
			regerror(ret, &re[i], error_msg, 128);
			Assert(ret == 0, "regex compilation failed: %s\n%s", error_msg, rules[i].regex);
		}
	}
}

typedef struct token {
	int type;
	char str[32];
} Token;

Token tokens[32];
int nr_token;

static bool make_token(char *e) {
	int position = 0;
	int i;
	regmatch_t pmatch;
	
	nr_token = 0;

	while(e[position] != '\0') {
		/* Try all rules one by one. */
		for(i = 0; i < NR_REGEX; i ++) {
			if(regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
				char *substr_start = e + position;
				int substr_len = pmatch.rm_eo;

				if (rules[i].token_type == VAR)
				{
					int j;
					for (j = 0; j < nr_symtab_entry; j++)
					{
						if (substr_len == strlen(strtab + symtab[j].st_name) && strncmp(strtab + symtab[j].st_name, substr_start, substr_len) == 0)
						{
							sprintf(tokens[nr_token].str, "0x%x", symtab[j].st_value);
							break;
						}
					}
					if (j == nr_symtab_entry) continue; //变量名未声明，匹配失败
				}

				Log_write("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i, rules[i].regex, position, substr_len, substr_len, substr_start); //如果需要输出调试信息，则把"Log_write"改成"Log"
				position += substr_len;

				/* Now a new token is recognized with rules[i]. Add codes
				 * to record the token in the array ``tokens''. For certain 
				 * types of tokens, some extra actions should be performed.
				 */
				
				switch(rules[i].token_type) {
					case NOTYPE: break;
					case REG: substr_start++; substr_len--; //使得'$'符号不会被记录在子串中
					case DECNUM: case HEXNUM:
						if (substr_len > 32) {puts("缓冲区溢出！检查输入的表达式是否含有超长的操作数。"); return false;}
						strncpy(tokens[nr_token].str, substr_start, substr_len); tokens[nr_token].str[substr_len] = 0;
					default: tokens[nr_token++].type = rules[i].token_type;
				}

				break;
			}
		}

		if(i == NR_REGEX) {
			printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
			return false;
		}
	}

	return true; 
}

bool check_kh(int p, int q)
{
	if (tokens[p].type != '(' || tokens[q].type != ')') return 0;
	int k = 1;
	for (int i = p+1; i < q; i++) //k表示当前在第几层括号中
	{
		if (tokens[i].type == ')') k--;
		if (tokens[i].type == '(') k++;
		if (k == 0) return 0; //最左边的括号和最右边的括号不是一对
	}
	return k == 1;
}

inline int op_level(int c)
{
	switch (c)
	{
		case '|': return 1;
		case '&': return 2;
		case '=': case NON_EQ: return 3;
		case '+': case '-': return 4;
		default: return 5; //case '*': case '/':
	}
}
int eval_tokens(int p, int q, bool* success)
{
	//printf("%d %d\n", p, q);
	if (!*success) return 0; //求值已经失败，不再继续往下求了
	if (p > q) {puts("不合法的表达式，请检查输入（空串、空括号等）。"); *success = 0; return 0;} //Bad expression
	else if (p == q) //Single number
	{
		if (tokens[p].type == DECNUM) return atoi(tokens[p].str);
		else if (tokens[p].type == HEXNUM)
		{
			int r;
			sscanf(tokens[p].str, "%x", &r);
			return r;
		}
		else {
			if (strcmp(tokens[p].str, "eip") == 0) return cpu.eip;
			for (int i = R_EAX; i <= R_EDI; i++)
				if (strcmp(tokens[p].str, regsl[i]) == 0) return reg_l(i);
			for (int i = R_AX; i <= R_DI; i++)
				if (strcmp(tokens[p].str, regsw[i]) == 0) return reg_w(i);
			for (int i = R_AL; i <= R_BH; i++)
				if (strcmp(tokens[p].str, regsb[i]) == 0) return reg_b(i);
			return 0;
		}
	}
		else if (check_kh(p, q)) return eval_tokens(p+1, q-1, success); //整个表达式在一对括号中
	else {
		int pos = -1, lev = 0x7fffffff;
		for (int i = p+1, dep = (tokens[p].type == '(' ? 1 : 0); i <= q; i++)
		{
			int cur = tokens[i].type;
			if (cur == DECNUM || cur == REG || cur == HEXNUM || cur == DEREF || cur == NEGATE || cur == NOT) continue;
			if (cur == '(') {dep++; continue;}
			if (cur == ')') {dep--; continue;}
			if (dep != 0) continue;
			if (op_level(cur) <= lev) {pos = i; lev = op_level(cur);}
		}
		//printf("%d\n", pos);
		if (pos != -1) 
		{
			int val1 = eval_tokens(p, pos-1, success);
			int val2 = eval_tokens(pos+1, q, success);
			switch (tokens[pos].type)
			{
				case '+': return val1 + val2;
				case '-': return val1 - val2;
				case '*': return val1 * val2;
				case '/': return val1 / val2;
				case '=': return val1 == val2;
				case NON_EQ: return val1 != val2;
				case '&': return val1 && val2;
				case '|': return val1 || val2;
				default: success = false; return 0; //不合法的运算符
			}
		}
		else {
			int c = eval_tokens(p+1, q, success);
			if (tokens[p].type == NEGATE) return -c;
			else if (tokens[p].type == NOT) return !c;
			else return swaddr_read(eval_tokens(p+1, q, success), 4);
		}
	}
}

uint32_t expr(char *e, bool *success) {
	if(!make_token(e)) {
		*success = false;
		return 0;
	}

	for (int i = 0; i < nr_token; i++)
	{
		if (tokens[i].type == VAR) tokens[i].type = HEXNUM; //目前在NEMU中是把变量直接视为一个地址的
		int prev = i == 0 ? NOTYPE : tokens[i-1].type;
		if (!(i > 0 && (prev == DECNUM || prev == HEXNUM || prev == REG || prev == ')')))
		{
			if (tokens[i].type == '-') tokens[i].type = NEGATE;
			if (tokens[i].type == '*') tokens[i].type = DEREF;
		}
		//printf("%d %s\n", tokens[i].type, tokens[i].str);
	}

	int val = eval_tokens(0, nr_token-1, success);
	return *success ? val : 0;
}

