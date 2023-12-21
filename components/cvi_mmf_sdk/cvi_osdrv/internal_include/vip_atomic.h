#ifndef __VI_ATOMIC_H__
#define __VI_ATOMIC_H__

#include "k_api.h"
#include "k_atomic.h"

static inline void atomic_set(atomic_t *v, int i)
{
	rhino_atomic_set(v, i);
}

static inline int atomic_read(const atomic_t *v)
{
	return rhino_atomic_get(v);
}

static inline void atomic_inc(atomic_t *v)
{
	rhino_atomic_inc(v);
}

static inline int atomic_cmpxchg(atomic_t *v, int old, int new_value)
{
	CPSR_ALLOC();
	int ret = *v;

	RHINO_CPU_INTRPT_DISABLE();

	if (*v == old) {
		*v = new_value;
		ret = old;
	}

	RHINO_CPU_INTRPT_ENABLE();

	return ret;
}

static inline void atomic_dec(atomic_t *v)
{
	rhino_atomic_dec(v);
}

#if 0
static inline int atomic_sub_return(int value, atomic_t *v)
{
	CPSR_ALLOC();
	int ret;

	RHINO_CPU_INTRPT_DISABLE();

	*v -= value;
	ret = *v;

	RHINO_CPU_INTRPT_ENABLE();

	return ret;
}

static inline int atomic_add_return(int value, atomic_t *v)
{
	CPSR_ALLOC();
	int ret;

	RHINO_CPU_INTRPT_DISABLE();

	*v += value;
	ret = *v;

	RHINO_CPU_INTRPT_ENABLE();

	return ret;
}
#endif
#endif
