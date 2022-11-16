#pragma once
#include <concepts>
#include "exo/macros/assert.h"
#include "exo/maths/numerics.h"

namespace refl
{

using ClassId = u64;

using CtorFunc     = void *(*)(void *); // void* ctor(void *memory);
using DtorFunc     = void (*)(void *);  // void dtor(void* ptr);
using RegisterFunc = void (*)();        // void register();

struct TypeInfo
{
	ClassId     class_id       = 0;       // unique class id
	const char *name           = nullptr; // name of the type
	TypeInfo   *base           = nullptr; // ptr to base type for inheritance
	usize       size           = 0;       // sizeof(T)
	CtorFunc    placement_ctor = nullptr; // call placement new, nullptr for abstract classes
	DtorFunc    dtor           = nullptr; // call destructor
};

namespace details
{

// Globals
inline constinit TypeInfo     types_storage[128] = {};
inline constinit RegisterFunc types_to_init[128] = {};
inline constinit usize        types_length       = 0;

// Helpers
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

constexpr ClassId hash(const char *str)
{
	u64 hash = 5381;
	for (const char *c = str; *c; ++c) {
		hash = ((hash << 5) + hash) + u64(*c); /* hash * 33 + c */
	}
	return hash;
}

// -- Type registering

// Used to initialize static type_info ptr on classes.
inline TypeInfo *defer_register(RegisterFunc func)
{
	types_to_init[types_length] = func;
	auto &res                   = types_storage[types_length];
	res.class_id                = types_length;
	++types_length;
	return &res;
}

// Actual initialization of the type info, should be called from `call_all_registers()`
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

// Initialize all types
inline void call_all_registers()
{
	for (usize i = 0; i < types_length; ++i) {
		types_to_init[i]();
	}
}

// -- Storage for reflection-based pointers
union PtrWithTypeInfo
{
	struct
	{
		u64 lo : 48; // pointer to object
		u64 hi : 16; // index into the types_storage array of the type_info corresponding to the pointed object

	} bits;
	u64 raw = 0;

	void from(void *ptr, const TypeInfo *type_info)
	{
		this->bits.lo = reinterpret_cast<u64>(ptr);
		this->bits.hi = type_info ? u64(type_info - &types_storage[0]) : 0;
	}
	void *ptr() const { return reinterpret_cast<void *>(bits.lo); }

	TypeInfo &type_info() const { return types_storage[bits.hi]; }
};
static_assert(sizeof(PtrWithTypeInfo) == sizeof(void *));

} // namespace details

// void* pointer with typeinfo on the pointed object
struct TypedPtr
{
	template <typename From>
	static TypedPtr from(From *ptr)
	{
		TypedPtr typed_ptr;
		typed_ptr.storage.from(ptr, From::TYPE_INFO);
		return typed_ptr;
	}

	// Query the type of the pointed object
	template <typename To>
	To *as()
	{
		if (&this->storage.type_info() == To::TYPE_INFO)
			return static_cast<To *>(this->storage.ptr());
		else
			return nullptr;
	}

	// Traverse the type hierarchy to upcast to parent class
	template <typename To>
	To *upcast()
	{
		const TypeInfo *cur_typeinfo = &this->storage.type_info();
		while (cur_typeinfo) {
			if (cur_typeinfo == To::TYPE_INFO)
				return static_cast<To *>(this->storage.ptr());

			cur_typeinfo = cur_typeinfo->base;
		}
		return nullptr;
	}

	usize get_size() const { return this->storage.type_info().size; }

private:
	details::PtrWithTypeInfo storage;
};

// Traverse the type hierarchy to upcast to parent class
template <typename Base>
Base *upcast(void *ptr, const TypeInfo *type_info)
{
	const TypeInfo *cur_typeinfo = type_info;
	while (cur_typeinfo) {
		if (cur_typeinfo == Base::TYPE_INFO)
			return static_cast<Base *>(ptr);
		cur_typeinfo = cur_typeinfo->base;
	}
	return nullptr;
}
template <typename Base, typename Derived>
Base *upcast(Derived *ptr)
{
	if constexpr (!std::derived_from<Derived, Base>) {
		return nullptr;
	} else {
		return upcast<Base>(static_cast<void *>(ptr), Derived::TYPE_INFO);
	}
}

// "virtual" pointer that can only points to one class hierarchy
template <typename Base>
struct BasePtr
{
	BasePtr() = default;

	template <std::derived_from<Base> Derived>
	explicit BasePtr(Derived *p_derived)
	{
		this->storage.from(p_derived, Derived::TYPE_INFO);
	}

	explicit BasePtr(Base *p_derived, const TypeInfo &typeinfo)
	{
		ASSERT(p_derived);
		this->storage.from(p_derived, &typeinfo);
	}

	// Copy from a derived class
	template <std::derived_from<Base> Derived>
	BasePtr &operator=(BasePtr<Derived> other)
	{
		this->storage = other.storage;
		return *this;
	}
	template <std::derived_from<Base> Derived>
	BasePtr(BasePtr<Derived> other)
	{
		*this = other.storage;
	}

	static BasePtr invalid()
	{
		BasePtr null;
		ASSERT(null.storage.raw == 0);
		return null;
	}

	Base *get() { return static_cast<Base *>(this->storage.ptr()); }
	bool  is_valid() const { return this->storage.raw != 0; }

	template <std::derived_from<Base> Derived>
	Derived *as()
	{
		if (&this->storage.type_info() == Derived::TYPE_INFO)
			return static_cast<Derived *>(this->storage.ptr());
		else
			return nullptr;
	}

	const TypeInfo &typeinfo() { return this->storage.type_info(); }

	inline bool  operator==(const BasePtr &other) const { return this->storage.raw == other.storage.raw; }
	inline Base *operator->() { return static_cast<Base *>(this->storage.ptr()); }

	details::PtrWithTypeInfo storage;
};

// Iterate on all types to find a typeinfo that matches the given class id
inline TypeInfo *get_type_info(ClassId class_id)
{
	for (usize i_type = 0; i_type < details::types_length; ++i_type) {
		if (details::types_storage[i_type].class_id == class_id) {
			return &details::types_storage[i_type];
		}
	}
	return nullptr;
}

// Returns the typeinfo of a type
template <typename T>
const TypeInfo &typeinfo()
{
	return *T::TYPE_INFO;
}

}; // namespace refl

#define REFL_REGISTER_TYPE(_name)                                                                                      \
	inline static auto *TYPE_INFO = refl::details::defer_register([]() { refl::details::register_type<Self>(_name); });

#define REFL_REGISTER_TYPE_WITH_SUPER(_name)                                                                           \
	inline static auto *TYPE_INFO =                                                                                    \
		refl::details::defer_register([]() { refl::details::register_type<Self>(_name, Super::TYPE_INFO); });
