#include "monitor/watchpoint.h"
#include "monitor/expr.h"
#include "cpu/reg.h"

#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;
static int tim = 0;

void init_wp_pool() {
	int i;
	for(i = 0; i < NR_WP; i ++) {
		wp_pool[i].NO = i;
		wp_pool[i].next = &wp_pool[i + 1];
	}
	wp_pool[NR_WP - 1].next = NULL;

	head = NULL;
	free_ = wp_pool;
}

//为了打印时有序，我们必须在插入和删除时保持head链表和free_链表的顺序
void insert_in_order(WP* cur, WP** head)
{
	if (*head == NULL || (*head) -> NO > cur -> NO)
	{
		cur -> next = *head; *head = cur;
		return;
	}
	WP* pos = *head;
	while (pos -> next != NULL && pos -> next -> NO < cur -> NO) pos = pos -> next;
	cur -> next = pos -> next; pos -> next = cur;
}
WP* new_wp()
{
	if (free_ == NULL) {puts("监视点数量已达上限，申请失败。"); return NULL;}
	WP* cur = free_; free_ = free_ -> next;
	insert_in_order(cur, &head); //cur -> next = head; head = cur;
	return cur;
}

void free_wp(WP* wp)
{
	if (wp == NULL) {puts("参数无效，不能以NULL作为参数。"); return;}
	if (wp == head) head = head -> next;
	else {
		WP* prev = head;
		while (prev -> next != wp) prev = prev -> next;
		if (prev == NULL) {puts("找不到指定的监视点，删除失败。"); return;}
		prev -> next = wp -> next;
	}
	insert_in_order(wp, &free_); //wp -> next = free_; free_ = wp;
}

int free_wp_no(int no)
{
	WP* pos = head;
	while (pos != NULL && pos -> NO != no) pos = pos -> next;
	if (pos == NULL) {printf("找不到监视点%d，请检查输入\n", no); return 3;}
	free_wp(pos);
	return 0;
}

void print_wp()
{
	if (head == NULL) puts("当前未设置任何监视点。");
	else {
		puts("编号\tCurrent Value 表达式");
		for (WP* i = head; i != NULL; i = i -> next)
			printf("%d\t%-10u    %s\n", i -> NO, i -> val, i -> exp);
	}
	printf("监视点累计触发次数：%d\n", tim);
}

bool check_wp()
{
	bool r = 1;
	for (WP* i = head; i != NULL; i = i -> next)
	{
		bool success = 1;
		uint32_t cur = expr(i -> exp, &success); //添加监视点时，已经确保了所有的表达式能成功求值
		if (cur != i -> val)
		{
			r = 0; tim++;
			printf("监视点%d：%s，旧值%u，新值%u\n", i -> NO, i -> exp, i -> val, cur);
			i -> val = cur;
		}
	}
	if (r == 0) printf("程序当前执行到地址0x%x。\n", cpu.eip);
	return r;
}

