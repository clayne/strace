/*
 * Copyright (c) 1999-2000 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 2002-2005 Roland McGrath <roland@redhat.com>
 * Copyright (c) 2008 Jan Kratochvil <jan.kratochvil@redhat.com>
 * Copyright (c) 2009-2013 Denys Vlasenko <dvlasenk@redhat.com>
 * Copyright (c) 2006-2015 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2014-2025 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"
#include "scno.h"
#include <linux/sched.h>

#include "xstring.h"
#include <unistd.h>

#include "xlat/clone_flags.h"
#include "xlat/clone3_flags.h"
#include "xlat/setns_types.h"
#include "xlat/unshare_flags.h"

#if defined IA64
# define ARG_FLAGS	0
# define ARG_STACK	1
# define ARG_STACKSIZE	(shuffle_scno(tcp->scno) == __NR_clone2 ? 2 : -1)
# define ARG_PTID	(shuffle_scno(tcp->scno) == __NR_clone2 ? 3 : 2)
# define ARG_CTID	(shuffle_scno(tcp->scno) == __NR_clone2 ? 4 : 3)
# define ARG_TLS	(shuffle_scno(tcp->scno) == __NR_clone2 ? 5 : 4)
#elif defined S390 || defined S390X
# define ARG_STACK	0
# define ARG_FLAGS	1
# define ARG_PTID	2
# define ARG_CTID	3
# define ARG_TLS	4
#elif defined X86_64 || defined X32
/* x86 personality processes have the last two arguments flipped. */
# define ARG_FLAGS	0
# define ARG_STACK	1
# define ARG_PTID	2
# define ARG_CTID	((current_personality != 1) ? 3 : 4)
# define ARG_TLS	((current_personality != 1) ? 4 : 3)
#elif defined ALPHA || defined TILE || defined OR1K || defined CSKY
# define ARG_FLAGS	0
# define ARG_STACK	1
# define ARG_PTID	2
# define ARG_CTID	3
# define ARG_TLS	4
#else
# define ARG_FLAGS	0
# define ARG_STACK	1
# define ARG_PTID	2
# define ARG_TLS	3
# define ARG_CTID	4
#endif

static bool show_namespace = false;
extern void namespace_auxstr_init(void)
{
	show_namespace = true;
}

/* derived from print_dirfd */
static const char *
read_namespace_id(int pid, const char *const ns_type)
{
	static const char ns_path[] = "/proc/%d/ns/%s";
	/* "cgroup" is the longest name in the dentries in /proc/$pid/ns. */
	char linkpath[sizeof(ns_path) + sizeof(int) * 3 + sizeof("cgroup")];
	xsprintf(linkpath, ns_path, pid, ns_type);

	static char buf[PATH_MAX + 1];
	ssize_t n = readlink(linkpath, buf, sizeof(buf));
	if ((size_t) n >= sizeof(buf))
		return NULL;
	buf[n] = '\0';
	return buf;
}

static const char *
get_namespace_auxstr(int pid, uint64_t flags,
		     struct tcb *const tcp_for_pid_translation)
{
	static char str[sizeof("cgroup:[4026531835], "
			          "ipc:[4026531839], "
			          "mnt:[4026531841], "
			          "net:[4026531840], "
			          "pid:[4026531836], "
			         "time:[4026531834], "
			         "user:[4026531837], "
			          "uts:[4026531838]" )];
	char *p = str;

	static const struct {
		uint64_t flags;
		const char *str;
	} ns[] = {
		{ CLONE_NEWCGROUP | CLONE_INTO_CGROUP, "cgroup" },
		{ CLONE_NEWIPC, "ipc" },
		{ CLONE_NEWNS, "mnt" },
		{ CLONE_NEWNET, "net" },
		{ CLONE_NEWPID, "pid" },
		{ CLONE_NEWTIME, "time" },
		{ CLONE_NEWUTS, "uts" },
		{ CLONE_NEWUSER, "user" },
	};

	if (tcp_for_pid_translation) {
		int proc_pid;
		if (translate_pid(tcp_for_pid_translation, pid, PT_TID, &proc_pid) == 0)
			return NULL;
		pid = proc_pid;
	}

	*p = '\0';
	for (size_t i = 0; i < ARRAY_SIZE(ns); ++i) {
		if (flags & ns[i].flags) {
			const char *ns_name = read_namespace_id(pid, ns[i].str);
			if (ns_name) {
				if (str != p)
					p = xappendstr(str, p, ", ");
				p = xappendstr(str, p, "%s", ns_name);
			}
		}
	}

	return (str != p) ? str : NULL;
}

static void
print_tls_arg(struct tcb *const tcp, const kernel_ulong_t addr)
{
#ifdef HAVE_STRUCT_USER_DESC
	if ((SUPPORTED_PERSONALITIES == 1) || (current_personality == 1))
	{
		print_user_desc(tcp, addr, USER_DESC_BOTH);
	}
	else
#endif /* HAVE_STRUCT_USER_DESC */
	{
		printaddr(addr);
	}
}

SYS_FUNC(clone)
{
	const kernel_ulong_t flags = tcp->u_arg[ARG_FLAGS] & ~CSIGNAL;
	const kernel_ulong_t nsflags = show_namespace
		? (CLONE_NEWCGROUP|CLONE_NEWIPC|CLONE_NEWNS|CLONE_NEWNET
		   |CLONE_NEWPID|CLONE_NEWTIME|CLONE_NEWUTS|CLONE_NEWUSER)
		: 0;
	int r_extra = 0;

	if (entering(tcp)) {
		const unsigned int sig = tcp->u_arg[ARG_FLAGS] & CSIGNAL;

		tprints_arg_name_begin("child_stack");
		printaddr(tcp->u_arg[ARG_STACK]);
		tprint_arg_name_end();
		tprint_arg_next();
#ifdef ARG_STACKSIZE
		if (ARG_STACKSIZE != -1) {
			tprints_arg_name_begin("stack_size");
			PRINT_VAL_X(tcp->u_arg[ARG_STACKSIZE]);
			tprint_arg_name_end();
			tprint_arg_next();
		}
#endif
		tprints_arg_name_begin("flags");
		if (flags) {
			tprint_flags_begin();
			printflags64_in(clone_flags, flags, "CLONE_???");
			if (sig) {
				tprint_flags_or();
				printsignal(sig);
			}
			tprint_flags_end();
		} else {
			printsignal(sig);
		}
		tprint_arg_name_end();
		/*
		 * TODO on syscall entry:
		 * We can clear CLONE_PTRACE here since it is an ancient hack
		 * to allow us to catch children, and we use another hack for that.
		 * But CLONE_PTRACE can conceivably be used by malicious programs
		 * to subvert us. By clearing this bit, we can defend against it:
		 * in untraced execution, CLONE_PTRACE should have no effect.
		 *
		 * We can also clear CLONE_UNTRACED, since it allows to start
		 * children outside of our control. At the moment
		 * I'm trying to figure out whether there is a *legitimate*
		 * use of this flag which we should respect.
		 */
		if ((flags & (CLONE_PARENT_SETTID|CLONE_PIDFD|CLONE_CHILD_SETTID
			      |CLONE_CHILD_CLEARTID|CLONE_SETTLS
			      |nsflags)) == 0)
			return RVAL_DECODED | RVAL_TID;
	} else {
		if (flags & (CLONE_PARENT_SETTID|CLONE_PIDFD)) {
			kernel_ulong_t addr = tcp->u_arg[ARG_PTID];

			tprint_arg_next();
			tprints_arg_name_begin("parent_tid");
			if (flags & CLONE_PARENT_SETTID)
				printnum_pid(tcp, addr, PT_TID);
			else
				printnum_fd(tcp, addr);
			tprint_arg_name_end();
		}
		if (flags & CLONE_SETTLS) {
			tprint_arg_next();
			tprints_arg_name_begin("tls");
			print_tls_arg(tcp, tcp->u_arg[ARG_TLS]);
			tprint_arg_name_end();
		}
		if (flags & (CLONE_CHILD_SETTID|CLONE_CHILD_CLEARTID)) {
			tprint_arg_next();
			tprints_arg_name_begin("child_tidptr");
			printaddr(tcp->u_arg[ARG_CTID]);
			tprint_arg_name_end();
		}
		if ((flags & nsflags) && !syserror(tcp)) {
			tcp->auxstr = get_namespace_auxstr(tcp->u_rval, flags, tcp);
			if (tcp->auxstr)
				r_extra = RVAL_STR;
		}
	}
	return RVAL_TID | r_extra;
}

static void
tprint_value_changed_struct_begin(void)
{
	tprint_value_changed();
	tprint_struct_begin();
}

SYS_FUNC(clone3)
{
	static const size_t minsz = offsetofend(struct clone_args, tls);

	const kernel_ulong_t addr = tcp->u_arg[0];
	const kernel_ulong_t size = tcp->u_arg[1];

	struct clone_args arg = { 0 };
	kernel_ulong_t fetch_size;

	fetch_size = MIN(size, sizeof(arg));

	const uint64_t nsflags = show_namespace
		? (CLONE_NEWCGROUP|CLONE_INTO_CGROUP|CLONE_NEWIPC|CLONE_NEWNS|CLONE_NEWNET
		   |CLONE_NEWPID|CLONE_NEWTIME|CLONE_NEWUTS|CLONE_NEWUSER)
		: 0;
	int r_extra = 0;

	if (entering(tcp)) {
		if (fetch_size < minsz) {
			printaddr(addr);
			goto out;
		} else if (umoven_or_printaddr(tcp, addr, fetch_size, &arg)) {
			goto out;
		}

		tprint_struct_begin();
		tprints_field_name("flags");
		tprint_flags_begin();
		printflags_ex(arg.flags, "CLONE_???", XLAT_STYLE_DEFAULT,
			      clone_flags, clone3_flags, NULL);
		tprint_flags_end();

		if (arg.flags & CLONE_PIDFD) {
			tprint_struct_next();
			PRINT_FIELD_ADDR64(arg, pidfd);
		}

		if (arg.flags & (CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID)) {
			tprint_struct_next();
			PRINT_FIELD_ADDR64(arg, child_tid);
		}

		if (arg.flags & CLONE_PARENT_SETTID) {
			tprint_struct_next();
			PRINT_FIELD_ADDR64(arg, parent_tid);
		}

		if (arg.exit_signal < INT_MAX) {
			tprint_struct_next();
			PRINT_FIELD_OBJ_VAL(arg, exit_signal, printsignal);
		} else {
			tprint_struct_next();
			PRINT_FIELD_U(arg, exit_signal);
		}

		tprint_struct_next();
		PRINT_FIELD_ADDR64(arg, stack);
		tprint_struct_next();
		PRINT_FIELD_X(arg, stack_size);

		if (arg.flags & CLONE_SETTLS) {
			tprint_struct_next();
			PRINT_FIELD_OBJ_TCB_VAL(arg, tls, tcp,
						print_tls_arg);
		}

		if (arg.set_tid || arg.set_tid_size) {
			static const unsigned int max_set_tid_size = 32;

			if (!arg.set_tid || !arg.set_tid_size ||
			    arg.set_tid_size > max_set_tid_size) {
				tprint_struct_next();
				PRINT_FIELD_ADDR64(arg, set_tid);
			} else {
				int buf;

				tprint_struct_next();
				PRINT_FIELD_OBJ_TCB_VAL(arg, set_tid, tcp,
					print_array, arg.set_tid_size,
					&buf, sizeof(buf), tfetch_mem,
					print_int_array_member, 0);
			}
			tprint_struct_next();
			PRINT_FIELD_U(arg, set_tid_size);
		}

		if (fetch_size > offsetof(struct clone_args, cgroup)
		    && (arg.cgroup || arg.flags & CLONE_INTO_CGROUP)) {
			tprint_struct_next();
			PRINT_FIELD_U(arg, cgroup);
		}

		if (size > fetch_size)
			print_nonzero_bytes(tcp, tprint_struct_next,
					    addr, fetch_size,
					    MIN(size, get_pagesize()),
					    QUOTE_FORCE_HEX);

		tprint_struct_end();

		if ((arg.flags & (CLONE_PIDFD | CLONE_PARENT_SETTID | nsflags)) ||
		    (size > fetch_size))
			return RVAL_TID;

		goto out;
	}

	/* exiting */

	if (syserror(tcp))
		goto out;

	if (umoven(tcp, addr, fetch_size, &arg)) {
		tprint_value_changed();
		printaddr(addr);
		goto out;
	}

	void (*prefix_fun)(void) = tprint_value_changed_struct_begin;

	if (arg.flags & CLONE_PIDFD) {
		prefix_fun();
		prefix_fun = tprint_struct_next;
		tprints_field_name("pidfd");
		printnum_fd(tcp, arg.pidfd);
	}

	if (arg.flags & CLONE_PARENT_SETTID) {
		prefix_fun();
		prefix_fun = tprint_struct_next;
		tprints_field_name("parent_tid");
		printnum_pid(tcp, arg.parent_tid, PT_TID);
	}

	if (size > fetch_size) {
		/*
		 * TODO: it is possible to also store the tail on entering
		 *       and then compare against it on exiting in order
		 *       to avoid double-printing, but it would also require yet
		 *       another complication of print_nonzero_bytes interface.
		 */
		if (print_nonzero_bytes(tcp, prefix_fun, addr, fetch_size,
					MIN(size, get_pagesize()),
					QUOTE_FORCE_HEX)) {
			prefix_fun = tprint_struct_next;
		}
	}

	if (prefix_fun != tprint_value_changed_struct_begin)
		tprint_struct_end();

	if ((arg.flags & nsflags) && !syserror(tcp)) {
		tcp->auxstr = get_namespace_auxstr(tcp->u_rval, arg.flags, tcp);
		if (tcp->auxstr)
			r_extra = RVAL_STR;
	}

out:
	tprint_arg_next();
	PRINT_VAL_U(size);

	return RVAL_DECODED | RVAL_TID | r_extra;
}


SYS_FUNC(setns)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprint_arg_next();
		printflags(setns_types, tcp->u_arg[1], "CLONE_NEW???");
		return show_namespace ? 0 : RVAL_DECODED;
	}

	int r_extra = 0;
	if (show_namespace && !syserror(tcp)) {
		tcp->auxstr = get_namespace_auxstr(tcp->pid, tcp->u_arg[1], NULL);
		if (tcp->auxstr)
			r_extra = RVAL_STR;
	}
	return RVAL_DECODED | r_extra;

}

SYS_FUNC(unshare)
{
	if (entering(tcp)) {
		printflags64(unshare_flags, tcp->u_arg[0], "CLONE_???");
		return show_namespace ? 0 : RVAL_DECODED;
	}

	int r_extra = 0;
	if (show_namespace && !syserror(tcp)) {
		tcp->auxstr = get_namespace_auxstr(tcp->pid, tcp->u_arg[0], NULL);
		if (tcp->auxstr)
			r_extra = RVAL_STR;
	}
	return RVAL_DECODED | r_extra;
}

SYS_FUNC(fork)
{
	return RVAL_DECODED | RVAL_TGID;
}
