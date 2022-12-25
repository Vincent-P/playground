#pragma once

#if defined(_WIN32)
#define DEBUG_BREAK() __debugbreak()
#else
#define DEBUG_BREAK() __asm__ volatile("int $0x03")
#endif
