#pragma once

#if defined(CXX_MSVC)
#define PACKED(struct_decl) __pragma(pack(push, 1)) struct_decl ; __pragma(pack(pop))
#else
#define PACKED(struct_decl) struct_decl __attribute__((packed)) ;
#endif
