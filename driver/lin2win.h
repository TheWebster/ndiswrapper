/*
 *  Copyright (C) 2006 Giridhar Pemmasani
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 */

#ifdef CONFIG_X86_64

u64 lin2win0(void *func);
u64 lin2win1(void *func, u64 arg1);
u64 lin2win2(void *func, u64 arg1, u64 arg2);
u64 lin2win3(void *func, u64 arg1, u64 arg2, u64 arg3);
u64 lin2win4(void *func, u64 arg1, u64 arg2, u64 arg3, u64 arg4);
u64 lin2win5(void *func, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5);
u64 lin2win6(void *func, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5,
	     u64 arg6);

#define LIN2WIN0(func)							\
({									\
	if (0)								\
		func();							\
	lin2win0(func);							\
})

#define LIN2WIN1(func, arg1)						\
({									\
	if (0)								\
		func(arg1);						\
	lin2win1(func, (u64)arg1);					\
})

#define LIN2WIN2(func, arg1, arg2)					\
({									\
	if (0)								\
		func(arg1, arg2);					\
	lin2win2(func, (u64)arg1, (u64)arg2);			\
})

#define LIN2WIN3(func, arg1, arg2, arg3)				\
({									\
	if (0)								\
		func(arg1, arg2, arg3);					\
	lin2win3(func, (u64)arg1, (u64)arg2, (u64)arg3);		\
})

#define LIN2WIN4(func, arg1, arg2, arg3, arg4)				\
({									\
	if (0)								\
		func(arg1, arg2, arg3, arg4);				\
	lin2win4(func, (u64)arg1, (u64)arg2, (u64)arg3, (u64)arg4);	\
})

#define LIN2WIN5(func, arg1, arg2, arg3, arg4, arg5)			\
({									\
	if (0)								\
		func(arg1, arg2, arg3, arg4, arg5);			\
	lin2win5(func, (u64)arg1, (u64)arg2, (u64)arg3, (u64)arg4,	\
		 (u64)arg5);						\
})

#define LIN2WIN6(func, arg1, arg2, arg3, arg4, arg5, arg6)		\
({									\
	if (0)								\
		func(arg1, arg2, arg3, arg4, arg5, arg6);		\
	lin2win6(func, (u64)arg1, (u64)arg2, (u64)arg3, (u64)arg4,	\
		 (u64)arg5, (u64)arg6);					\
})

#else // CONFIG_X86_64

#define LIN2WIN1(func, arg1)						\
({									\
	TRACE6("calling %p", func);					\
	func(arg1);							\
})
#define LIN2WIN2(func, arg1, arg2)					\
({									\
	TRACE6("calling %p", func);					\
	func(arg1, arg2);						\
})
#define LIN2WIN3(func, arg1, arg2, arg3)				\
({									\
	TRACE6("calling %p", func);					\
	func(arg1, arg2, arg3);						\
})
#define LIN2WIN4(func, arg1, arg2, arg3, arg4)				\
({									\
	TRACE6("calling %p", func);					\
	func(arg1, arg2, arg3, arg4);					\
})
#define LIN2WIN5(func, arg1, arg2, arg3, arg4, arg5)			\
({									\
	TRACE6("calling %p", func);					\
	func(arg1, arg2, arg3, arg4, arg5);				\
})
#define LIN2WIN6(func, arg1, arg2, arg3, arg4, arg5, arg6)		\
({									\
	TRACE6("calling %p", func);					\
	func(arg1, arg2, arg3, arg4, arg5, arg6);			\
})

#endif // CONFIG_X86_64
