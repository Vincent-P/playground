#pragma once
#include <concepts>
#include <exo/maths/numerics.h>

namespace refl
{
struct TypeInfo
{
	u64         class_id = 0;
	const char *name     = nullptr;
	TypeInfo   *base     = nullptr;
	usize       size     = 0;
};

inline static TypeInfo NULL_TYPE_INFO = {};

// Hash the user-provided class name to create a class id
constexpr u64 hash(const char *str)
{
	u64 hash = 5381;
	for (const char *c = str; *c; ++c) {
		hash = ((hash << 5) + hash) + u64(*c); /* hash * 33 + c */
	}
	return hash;
}

// -- Global init
using RegisterFunc = void (*)();
inline RegisterFunc types_to_init[128];
inline size_t       i_types_to_init = 0;

inline TypeInfo defer_register(RegisterFunc func)
{
	types_to_init[i_types_to_init++] = func;
	return {};
}

template <typename T>
void register_type(const char *name, TypeInfo *base = nullptr)
{
	T::TYPE_INFO.class_id = hash(name);
	T::TYPE_INFO.name     = name;
	T::TYPE_INFO.base     = base;
	T::TYPE_INFO.size     = sizeof(T);
}

inline void call_all_registers()
{
	for (size_t i = 0; i < i_types_to_init; ++i) {
		types_to_init[i]();
	}
}

// -- Type-erased ptr
struct TypedPtr
{
	void     *ptr;
	TypeInfo *type_info;

	template <typename From>
	static TypedPtr from(From *ptr)
	{
		TypedPtr typed_ptr;
		typed_ptr.ptr       = ptr;
		typed_ptr.type_info = &From::TYPE_INFO;
		return typed_ptr;
	}

	template <typename To>
	To *as()
	{
		if (this->type_info == &To::TYPE_INFO)
			return static_cast<To *>(this->ptr);
		else
			return nullptr;
	}

	template <typename To>
	To *upcast()
	{
		TypeInfo *cur_typeinfo = this->type_info;
		while (cur_typeinfo) {
			if (cur_typeinfo == &To::TYPE_INFO)
				return static_cast<To *>(this->ptr);

			cur_typeinfo = cur_typeinfo->base;
		}
		return nullptr;
	}

	size_t get_size() const { return type_info->size; }
};

// Upcast class hierarchy without TypedPtr
template <typename Base, typename Derived>
Base *upcast(Derived *ptr)
{
	if constexpr (!std::derived_from<Derived, Base>) {
		return nullptr;
	} else {
		TypeInfo *cur_typeinfo = &Derived::TYPE_INFO;
		while (cur_typeinfo) {
			if (cur_typeinfo == &Base::TYPE_INFO)
				return reinterpret_cast<Base *>(ptr);
			cur_typeinfo = cur_typeinfo->base;
		}
		return nullptr;
	}
}

// Downcast cast hierarchy without TypedPtr
template <typename Derived, typename Base>
Derived *downcast(Base *ptr)
{
	if constexpr (!std::derived_from<Derived, Base>) {
		return nullptr;
	} else {
		TypeInfo *cur_typeinfo = &Derived::TYPE_INFO;
		while (cur_typeinfo) {
			if (cur_typeinfo == &Base::TYPE_INFO)
				return reinterpret_cast<Derived *>(ptr);
			cur_typeinfo = cur_typeinfo->base;
		}
		return nullptr;
	}
}

// -- Pointer restricted to a hierarchy
template <typename Base>
struct BasePtr
{
	template <typename Derived>
	explicit BasePtr(Derived *p_derived)
	{
		static_assert(std::derived_from<Derived, Base>);
		Base *p_base = upcast<Base>(p_derived);
		if (p_base == nullptr) {
			this->ptr       = nullptr;
			this->type_info = &NULL_TYPE_INFO;
		} else {
			this->ptr       = static_cast<Base *>(p_derived);
			this->type_info = &Derived::TYPE_INFO;
		}
	}

	Base *get() { return this->ptr; }

	template <typename Derived>
	Derived *as()
	{
		if (this->type_info == &Derived::TYPE_INFO)
			return static_cast<Derived *>(this->ptr);
		else
			return nullptr;
	}

	const TypeInfo &typeinfo() { return *this->type_info; }

	inline bool  operator==(const BasePtr &other) const { return this->ptr == other.ptr; }
	inline Base *operator->() { return this->ptr; }

private:
	Base     *ptr;
	TypeInfo *type_info;
};

template <typename T>
const TypeInfo &typeinfo()
{
	return T::TYPE_INFO;
}

}; // namespace refl

#define REFL_REGISTER_TYPE(_name)                                                                                      \
	inline static auto TYPE_INFO = refl::defer_register([]() { refl::register_type<Self>(_name); });

#define REFL_REGISTER_TYPE_WITH_SUPER(_name)                                                                           \
	inline static auto TYPE_INFO = refl::defer_register([]() { refl::register_type<Self>(_name, &Super::TYPE_INFO); });
