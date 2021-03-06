/*
 * Copyright (c) 2017-2021 Dmitry V. Levin <ldv@strace.io>
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef STRACE_RT_SIGFRAME_H
# define STRACE_RT_SIGFRAME_H

# include "ptrace.h"
# include <signal.h>

typedef struct {
	struct sparc_stackf	ss;
	siginfo_t		info;
	struct pt_regs		regs;
	sigset_t		mask;
	/* more data follows */
} struct_rt_sigframe;

# define OFFSETOF_SIGMASK_IN_RT_SIGFRAME	\
		offsetof(struct_rt_sigframe, mask)

#endif /* !STRACE_RT_SIGFRAME_H */
