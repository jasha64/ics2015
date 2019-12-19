#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
	int NO;
	struct watchpoint *next;
	char exp[128];
	uint32_t val;
} WP;

WP* new_wp();
void free_wp(WP* wp);
int free_wp_no(int no);
void print_wp();
bool check_wp();

#endif
