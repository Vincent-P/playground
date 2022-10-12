#pragma once

namespace exo
{
/**
   Allows to store a PImpl inline without an indirection.
   It is intented to be used in platform code where there is only one PImpl that will be compiled. (Either win32 or
 linux for example) A maximum size must be specified. Using a struct that does not fit will not compile anwyay.

   Usage:

   // header file
   struct Window
   {
       struct Impl;
       exo::ForwardContainer<Impl> impl_container;
   };

   // in source file
   struct Window::Impl
   {
       HANDLE hwnd;
   };

   void something(Window &window)
   {
       HANDLE hwnd = window.impl_container.get().hwnd;
   }

 **/
template <typename ForwardedType, size_t MAX_SIZE = 4 * sizeof(void *)>
struct ForwardContainer
{
	const ForwardedType &get() const
	{
		static_assert(sizeof(ForwardedType) <= MAX_SIZE);
		return *reinterpret_cast<const ForwardedType *>(&bytes[0]);
	}

	ForwardedType &get()
	{
		static_assert(sizeof(ForwardedType) <= MAX_SIZE);
		return *reinterpret_cast<ForwardedType *>(&bytes[0]);
	}

	unsigned char bytes[MAX_SIZE];
};
}; // namespace exo
