/*
 * Copyright (c) 2016-2025 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_epoll_wait

# include <stdio.h>
# include <unistd.h>
# include <linux/eventpoll.h>

int
main(void)
{
	TAIL_ALLOC_OBJECT_CONST_PTR(struct epoll_event, ev);

	long rc = syscall(__NR_epoll_wait, -1, ev, 1, -2);
	printf("epoll_wait(-1, %p, 1, -2) = %s\n", ev, sprintrc(rc));

	puts("+++ exited with 0 +++");
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_epoll_wait")

#endif
