#pragma once
#include <concepts>
#include <exo/macros/assert.h>
#include <exo/maths/numerics.h>
#include <utility>

namespace exo
{
enum struct ResultType : u8
{
	Default,
	Ok,
	Err,
};

struct ErrorWrapper
{
	u64 code = u64_invalid;
};

template <typename T>
struct Result;
template <typename T>
inline Result<T> Ok(T &&value);
template <typename T, typename E>
inline Result<T> Err(E error);

template <std::default_initializable T>
struct Result<T>
{
	ResultType   type          = ResultType::Default;
	T            inner         = {};
	ErrorWrapper error_wrapper = {};

	Result<T> &operator=(Result<T> &&moved) noexcept
	{
		this->type               = moved.type;
		this->inner              = moved.inner;
		this->error_wrapper.code = moved.error_wrapper.code;
		return *this;
	}
	Result(Result<T> &&moved) { *this = std::move(moved); }

	template <typename OtherValue>
	Result<T> &operator=(Result<OtherValue> &&moved) noexcept
	{
		ASSERT(this->type == ResultType::Default);
		ASSERT(moved.type == ResultType::Err);
		this->type               = moved.type;
		this->error_wrapper.code = moved.error_wrapper.code;
		return *this;
	}
	template <typename OtherValue>
	Result(Result<OtherValue> &&moved)
	{
		*this = std::move(moved);
	}

	T &value()
	{
		ASSERT(this->type == ResultType::Ok);
		return inner;
	}

	ErrorWrapper error() const
	{
		ASSERT(this->type == ResultType::Err);
		return error_wrapper;
	}

	operator bool() const { return this->type == ResultType::Ok; }

private:
	explicit Result(T &&inner)
	{
		this->type               = ResultType::Ok;
		this->inner              = std::move(inner);
		this->error_wrapper.code = u64_invalid;
	}

	explicit Result(const T &inner)
	{
		this->type               = ResultType::Ok;
		this->inner              = inner;
		this->error_wrapper.code = u64_invalid;
	}

	Result(ErrorWrapper error)
	{
		this->type          = ResultType::Err;
		this->error_wrapper = error_wrapper;
	}

	template <typename Other, typename E>
	friend inline Result<Other> Err(E error);

	template <typename Other>
	friend inline Result<Other> Ok(Other &&value);
};

template <>
struct Result<void>
{
	ResultType   type          = ResultType::Default;
	ErrorWrapper error_wrapper = {};

	operator bool() const { return this->type == ResultType::Ok; }

	Result(Result<void> &&moved) noexcept
	{
		this->type               = moved.type;
		this->error_wrapper.code = moved.error_wrapper.code;
	}

	template <typename OtherValue>
	Result(Result<OtherValue> &&moved)
	{
		ASSERT(this->type == ResultType::Default);
		ASSERT(moved.type == ResultType::Err);
		this->type               = moved.type;
		this->error_wrapper.code = moved.error_wrapper.code;
	}

	ErrorWrapper error() const
	{
		ASSERT(this->type == ResultType::Err);
		return error_wrapper;
	}

private:
	explicit Result()
	{
		this->type               = ResultType::Ok;
		this->error_wrapper.code = u64_invalid;
	}

	explicit Result(ErrorWrapper error)
	{
		this->type          = ResultType::Err;
		this->error_wrapper = error;
	}

	template <typename T, typename E>
	friend Result<T>    Err(E error);
	friend Result<void> Ok();
};

template <typename T, typename E>
inline Result<T> Err(E error)
{
	u64 code = static_cast<u64>(error);
	return Result<T>(ErrorWrapper{code});
}

// error C2641: cannot deduce template arguments for 'exo::Result'
template <typename T>
Result(T &&moved) -> Result<T>;

template <typename T>
inline Result<T> Ok(T &&value)
{
	return Result(std::forward<T>(value));
}

inline Result<void> Ok() { return Result<void>(); }

} // namespace exo

using exo::Err;
using exo::Ok;
using exo::Result;
