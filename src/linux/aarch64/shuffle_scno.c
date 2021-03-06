/*
 * Copyright (c) 2015-2021 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define shuffle_scno_is_static
#define shuffle_scno arm_shuffle_scno
#include "../arm/shuffle_scno.c"
#undef shuffle_scno
#undef shuffle_scno_is_static

kernel_ulong_t
shuffle_scno(kernel_ulong_t scno)
{
	if (current_personality == 1)
		return arm_shuffle_scno(scno);

	return scno;
}
