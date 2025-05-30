/*
 * Copyright (c) 2015 Mike Frysinger <vapier@gentoo.org>
 * Copyright (c) 2016-2020 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2016-2021 Eugene Syromyatnikov <evgsyr@gmail.com>
 * Copyright (c) 2017 Chen Jingpiao <chenjingpiao@gmail.com>
 * Copyright (c) 2018 Paul Chaignon <paul.chaignon@gmail.com>
 * Copyright (c) 2021-2025 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"

/* for __X32_SYSCALL_BIT */
#include "scno.h"

/* PERSONALITY*_AUDIT_ARCH definitions depend on AUDIT_ARCH_* constants.  */
#include <linux/audit.h>
#define XLAT_MACROS_ONLY
# include "xlat/elf_em.h"
# include "xlat/audit_arch.h"
#undef XLAT_MACROS_ONLY

#include "nr_prefix.c"

const struct audit_arch_t audit_arch_vec[SUPPORTED_PERSONALITIES] = {
	PERSONALITY0_AUDIT_ARCH,
#if SUPPORTED_PERSONALITIES > 1
	PERSONALITY1_AUDIT_ARCH,
# if SUPPORTED_PERSONALITIES > 2
	PERSONALITY2_AUDIT_ARCH,
# endif
#endif
};


const char *
syscall_name_arch(unsigned long long ull_nr, unsigned int arch,
		  const char **prefix)
{
	const kernel_ulong_t nr = (kernel_ulong_t) ull_nr;

	if (nr == ull_nr) {
		for (size_t i = 0; i < SUPPORTED_PERSONALITIES; i++) {
			if (arch != audit_arch_vec[i].arch)
				continue;

			kernel_ulong_t scno = shuffle_scno_pers(nr, i);
			if (!scno_pers_is_valid(scno, i))
				continue;

			if (prefix) {
				*prefix = (i == current_personality)
					  ? nr_prefix(nr) : NULL;
			}
			return sysent_vec[i][scno].sys_name;
		}
	}

	if (prefix)
		*prefix = NULL;
	return NULL;
}
