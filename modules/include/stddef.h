#ifndef __STDDEF
#define __STDDEF

#define NULL ((void *)0)

typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __WCHAR_TYPE__ wchar_t;

#define offsetof(TYPE, MEMBER) __builtin_offsetof((TYPE), (MEMBER))

#endif
