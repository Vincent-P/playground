#pragma once

struct Tester
{
	void bark();
	void inline_bark() { a = 3; }
	int a = 42;
};

Tester make_tester(int a = 0);
