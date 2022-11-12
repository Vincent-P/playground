#pragma once
#include <utility>

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

struct Alive
{
	Alive() = default;
	explicit Alive(int *new_count) : count{new_count} { this->inc(); }

	Alive(const Alive &other) { *this = other; }
	Alive &operator=(const Alive &other)
	{
		this->count = other.count;
		this->inc();
		return *this;
	}

	Alive(Alive &&other) { *this = std::move(other); }
	Alive &operator=(Alive &&other)
	{
		this->count = other.count;
		other.count = nullptr;
		return *this;
	}

	~Alive()
	{
		this->dec();
		this->count = nullptr;
	}

private:
	void inc()
	{
		if (this->count) {
			*this->count = *this->count + 1;
		}
	}

	void dec()
	{
		if (this->count) {
			*this->count = *this->count - 1;
		}
	}

	int *count = nullptr;
};