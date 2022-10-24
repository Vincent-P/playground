#pragma once
#include <concepts>
#include <exo/maths/numerics.h>

namespace refl
{

using CtorFunc     = void *(*)(void *); // void* ctor(void *memory);
using DtorFunc     = void (*)(void *);  // void dtor(void* ptr);
using RegisterFunc = void (*)();        // void register();

using ClassId = u64;

struct TypeInfo
{
	ClassId     class_id       = 0;       // unique class id
	const char *name           = nullptr; // name of the type
	TypeInfo   *base           = nullptr; // ptr to base type for inheritance
	usize       size           = 0;       // sizeof(T)
	CtorFunc    placement_ctor = nullptr; // call placement new, nullptr for abstract classes
	DtorFunc    dtor           = nullptr; // call destructor
};

// Global storage
inline constinit TypeInfo     types_storage[128] = {};
inline constinit RegisterFunc types_to_init[128] = {};
inline constinit usize        types_length       = 0;

template <typename T>
inline void *default_placement_new(void *memory)
{
	return static_cast<void *>(new (memory)(T));
}

template <typename T>
inline void dtor(void *ptr)
{
	static_cast<T *>(ptr)->~T();
}

// Hash the user-provided class name to create a class id
constexpr ClassId hash(const char *str)
{
	u64 hash = 5381;
	for (const char *c = str; *c; ++c) {
		hash = ((hash << 5) + hash) + u64(*c); /* hash * 33 + c */
	}
	return hash;
}

// -- Global init
inline TypeInfo *defer_register(RegisterFunc func)
{
	types_to_init[types_length] = func;
	auto &res                   = types_storage[types_length];
	res.class_id                = types_length;
	++types_length;
	return &res;
}

template <typename T>
void register_type(const char *name, TypeInfo *base = nullptr)
{
	T::TYPE_INFO->class_id = hash(name);
	T::TYPE_INFO->name     = name;
	T::TYPE_INFO->base     = base;
	T::TYPE_INFO->size     = sizeof(T);
	if constexpr (std::is_constructible_v<T>) {
		T::TYPE_INFO->placement_ctor = &default_placement_new<T>;
	} else {
		T::TYPE_INFO->placement_ctor = nullptr;
	}
	T::TYPE_INFO->dtor = &dtor<T>;
}

inline void call_all_registers()
{
	for (usize i = 0; i < types_length; ++i) {
		types_to_init[i]();
	}
}

// -- Type-erased ptr
struct TypedPtr
{
	union PtrTypeInfo
	{
		u64 lo : 48;
		u64 hi : 16;

		void     *ptr() const { return reinterpret_cast<void *>(lo); }
		TypeInfo &type_info() const { return types_storage[hi]; }
	} bits;

	template <typename From>
	static TypedPtr from(From *ptr)
	{
		TypedPtr typed_ptr;
		typed_ptr.bits.lo = reinterpret_cast<u64>(ptr);
		typed_ptr.bits.hi = From::TYPE_INFO - &types_storage[0];
		return typed_ptr;
	}

	template <typename To>
	To *as()
	{
		if (&this->bits.type_info() == To::TYPE_INFO)
			return static_cast<To *>(this->bits.ptr());
		else
			return nullptr;
	}

	template <typename To>
	To *upcast()
	{
		const TypeInfo *cur_typeinfo = &this->bits.type_info();
		while (cur_typeinfo) {
			if (cur_typeinfo == To::TYPE_INFO)
				return static_cast<To *>(this->bits.ptr());

			cur_typeinfo = cur_typeinfo->base;
		}
		return nullptr;
	}

	usize get_size() const { return this->bits.type_info().size; }
};

// Upcast class hierarchy without TypedPtr
template <typename Base, typename Derived>
Base *upcast(Derived *ptr)
{
	if constexpr (!std::derived_from<Derived, Base>) {
		return nullptr;
	} else {
		TypeInfo *cur_typeinfo = Derived::TYPE_INFO;
		while (cur_typeinfo) {
			if (cur_typeinfo == Base::TYPE_INFO)
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
		TypeInfo *cur_typeinfo = Derived::TYPE_INFO;
		while (cur_typeinfo) {
			if (cur_typeinfo == Base::TYPE_INFO)
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
			this->type_info = &types_storage[0];
		} else {
			this->ptr       = static_cast<Base *>(p_derived);
			this->type_info = Derived::TYPE_INFO;
		}
	}

	Base *get() { return this->ptr; }

	template <typename Derived>
	Derived *as()
	{
		if (this->type_info == Derived::TYPE_INFO)
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

TypeInfo *get_type_info(ClassId class_id)
{
	for (usize i_type = 0; i_type < types_length; ++i_type) {
		if (types_storage[i_type].class_id == class_id) {
			return &types_storage[i_type];
		}
	}
	return nullptr;
}

template <typename T>
const TypeInfo &typeinfo()
{
	return *T::TYPE_INFO;
}

}; // namespace refl

#define REFL_REGISTER_TYPE(_name)                                                                                      \
	inline static auto *TYPE_INFO = refl::defer_register([]() { refl::register_type<Self>(_name); });

#define REFL_REGISTER_TYPE_WITH_SUPER(_name)                                                                           \
	inline static auto *TYPE_INFO = refl::defer_register([]() { refl::register_type<Self>(_name, Super::TYPE_INFO); });
