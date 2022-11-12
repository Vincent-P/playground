#pragma once

struct DtorCalled
{
	~DtorCalled()
	{
		this->i           = int(0xdeadbeef);
		this->dtor_called = true;
	}

	bool dtor_has_been_called() { return this->i == int(0xdeadbeef) && this->dtor_called; }

	int  i           = 42;
	bool dtor_called = false;
};
