#include "common.h"
#include <cstdio>

void Tester::bark() { printf("ahah: %d\n", this->a); }

Tester make_tester(int a)
{
	Tester tester = {};
	tester.a = a;
	return tester;
}
