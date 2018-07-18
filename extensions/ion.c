/* ion.c - crash extension to dump ion heaps infomation
 *
 * Copyright (C) 2018 Xiaomi, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "defs.h"      /* From the crash source top-level directory */

void ion_init(void);    /* constructor function */
void ion_fini(void);    /* destructor function (optional) */

void cmd_ion(void);     /* Declare the commands and their help data. */
char *help_ion[];

enum {
	DUMP_BUFFER = 0,
	DUMP_CLIENT,
};

static ulong dump_flags = 0;

#define need_dump(flags)						\
	((1<<flags) & dump_flags)

#define set_dump(flags)							\
	do {										\
		dump_flags |= (1 << (flags));			\
	}while(0)

static struct command_table_entry command_table[] = {
	{ "ion", cmd_ion, help_ion, 0},          /* One or more commands, */
	{ NULL },                                     /* terminated by NULL, */
};


void __attribute__((constructor))
ion_init(void) /* Register the command set. */
{
	register_extension(command_table);
}

/*
 *  This function is called if the shared object is unloaded.
 *  If desired, perform any cleanups here.
 */
void __attribute__((destructor))
ion_fini(void) { }

#define DUMP_PRINTF(filep, format, args...)		\
	do {										\
		fprintf((filep), format, ##args);		\
	} while(0)

#define DUMP_PRINTF_N_PREFIX(prefix, num, filep, format, args...)	\
	do {															\
		int i =0;													\
		for (i = 0; i < (num); i++) {								\
			fprintf((filep), "%s", (prefix));						\
		}															\
		fprintf((filep), format, ##args);							\
	} while(0)

struct ion_dump_cb_arg {
	char *prefix;
	int level;
};

static void dump_string(ulong addr, char *struct_name, char *member) {
	ulong tmp_addr;
	char str_buf[1024];

	readmem(addr + MEMBER_OFFSET(struct_name, member), KVADDR, &tmp_addr, sizeof(tmp_addr), "readmem_ul", FAULT_ON_ERROR);

	read_string(tmp_addr, str_buf, sizeof(str_buf));
	DUMP_PRINTF(fp, "%s", str_buf);
}

static ulong get_ulong(ulong addr) {
	ulong tmp_addr;

	readmem(addr, KVADDR, &tmp_addr, sizeof(tmp_addr), "readmem_ul", FAULT_ON_ERROR);
	return tmp_addr;
}

ulong total_size = 0;

int dump_one_buffer(char *prefix, int num, ulong addr) {
	ulong this_size;

	if (need_dump(DUMP_BUFFER))
		DUMP_PRINTF_N_PREFIX(prefix, num, fp, "ion_buffer:%lx\n", addr);

	if (!IS_KVADDR(addr)) {
		if (need_dump(DUMP_BUFFER))
			DUMP_PRINTF_N_PREFIX(prefix, num, fp, "invalid ion_buffer ptr:%lx\n", addr);
		return 1;
	}

	this_size = get_ulong(addr +
								MEMBER_OFFSET("ion_buffer", "size"));

	total_size += this_size;

	if (need_dump(DUMP_BUFFER)) {
		DUMP_PRINTF_N_PREFIX(prefix, num, fp, "size:");
		this_size = this_size/1024;  //KB
		if (this_size > 1024) {
			this_size = this_size / 1024; //MB
			DUMP_PRINTF(fp, "%luMB", this_size);
			if (this_size > 1024) {
				DUMP_PRINTF(fp, "i.e. %lu GB", this_size/1024);
			}
		} else {
			DUMP_PRINTF(fp, "%luKB", this_size);
		}
		DUMP_PRINTF(fp, "\n");
	}

	return 0;
}

int dump_one_handle(ulong addr, void *arg) {
	struct ion_dump_cb_arg *args = arg;
	char *prefix = NULL;
	int num = 0;

	if (args) {
		prefix = args->prefix;
		num = args->level;
	}

	dump_one_buffer(prefix, num,
					get_ulong(addr +
							  MEMBER_OFFSET("ion_handle", "buffer")));
	return 0;
}

int dump_buffers(char *prefix, int num, ulong addr) {
	struct callback_func_arg cb;
	struct ion_dump_cb_arg args;
	struct tree_data tree_data, *td;
	td = &tree_data;
	BZERO(td, sizeof(struct tree_data));
	td->start = addr;

	td->node_member_offset = (MEMBER_OFFSET("ion_handle", "node"));
	td->flags |= TREE_NODE_OFFSET_ENTERED;

	args.prefix = prefix;
	args.level = num;
	cb.arg = &args;
	cb.callback = dump_one_handle;

    do_rbtree(td, &cb);

	return 0;
}

int dump_one_client(ulong addr, void *arg) {
	struct ion_dump_cb_arg *args = arg;
	char *prefix = NULL;
	int num = 0;

	if (args) {
		prefix = args->prefix;
		num = args->level;
	}

	total_size = 0;
	DUMP_PRINTF_N_PREFIX(prefix, num, fp, "----------------- ion_client ---------------\n");
	DUMP_PRINTF_N_PREFIX(prefix, num, fp, "ion_client:%lx\n", addr);
	DUMP_PRINTF_N_PREFIX(prefix, num, fp, "name:");
	dump_string(addr, "ion_client", "name");
	DUMP_PRINTF(fp, "\n");
	DUMP_PRINTF_N_PREFIX(prefix, num, fp, "display_name:");
	dump_string(addr, "ion_client", "display_name");
	DUMP_PRINTF(fp, "\n");
	DUMP_PRINTF_N_PREFIX(prefix, num, fp, "pid:%lu\n",
			get_ulong(addr +
					  MEMBER_OFFSET("ion_client", "pid")));

	dump_buffers(prefix, num+1, addr +
				 MEMBER_OFFSET("ion_client", "handles"));
	DUMP_PRINTF_N_PREFIX(prefix, num, fp, "total used:%lu byte ", total_size);
	total_size = total_size/1024;  //KB
	if (total_size > 1024) {
		total_size = total_size / 1024; //MB
		DUMP_PRINTF(fp, " %luMB", total_size);
		if (total_size > 1024) {
			DUMP_PRINTF(fp, " i.e. %lu GB", total_size/1024);
		}
	} else {
		DUMP_PRINTF(fp, " %luKB", total_size);
	}
	DUMP_PRINTF(fp, "\n");

	return 0;
}

int dump_clients(char *prefix, int num, ulong addr) {
	struct callback_func_arg cb;
	struct ion_dump_cb_arg args;
	struct tree_data tree_data, *td;
	td = &tree_data;
	BZERO(td, sizeof(struct tree_data));

	td->start = addr;

	td->node_member_offset = (MEMBER_OFFSET("ion_client", "node"));
	td->flags |= TREE_NODE_OFFSET_ENTERED;

	args.prefix = prefix;
	args.level = num;
	cb.arg = &args;
	cb.callback = dump_one_client;

    do_rbtree(td, &cb);
    return 0;
}

int dump_ion_clients(char *prefix, int num, ulong addr) {
	dump_clients(prefix, num, addr);
	return 0;
}

int dump_one_heap(char *prefix, int num, ulong addr) {
	ulong clients_node;
	DUMP_PRINTF_N_PREFIX(prefix, num, fp, "---------- ion heap ---------------\n");
	DUMP_PRINTF_N_PREFIX(prefix, num, fp, "ion_heap:%lx\n", addr);
	DUMP_PRINTF_N_PREFIX(prefix, num, fp, "name:");
	dump_string(addr, "ion_heap", "name");
	DUMP_PRINTF(fp, "\n");

	DUMP_PRINTF_N_PREFIX(prefix, num, fp, "total_allocated:%ld\n",
			get_ulong(addr +
					  MEMBER_OFFSET("ion_heap", "total_allocated") +
					  MEMBER_OFFSET("atomic_t", "counter") ));
	DUMP_PRINTF_N_PREFIX(prefix, num, fp, "total_handles:%ld\n",
			get_ulong(addr +
					  MEMBER_OFFSET("ion_heap", "total_handles") +
					  MEMBER_OFFSET("atomic_t", "counter") ));

	clients_node = get_ulong(addr +MEMBER_OFFSET("ion_heap", "dev"));
	DUMP_PRINTF_N_PREFIX(prefix, num, fp, "ion_device:%lx\n", clients_node);
	clients_node = clients_node + MEMBER_OFFSET("ion_device", "clients");
	DUMP_PRINTF_N_PREFIX(prefix, num, fp, "clients_node:%lx\n", clients_node);
	if (need_dump(DUMP_CLIENT))
		dump_ion_clients(prefix, num +1, clients_node);
	return 0;
}

int dump_ion_heaps(char *prefix, int level, ulong addr) {
	ulong num_heaps;
	ulong p_heaps;
	ulong heap;
	int i = 0;

	if (!symbol_exists("num_heaps") || !symbol_exists("heaps"))
		return FALSE;

	num_heaps = symbol_value("num_heaps");
	num_heaps = get_ulong(num_heaps);
	p_heaps = symbol_value("heaps");
	p_heaps = get_ulong(p_heaps);
	DUMP_PRINTF_N_PREFIX(prefix, level, fp, "num_heaps:%lx\n", num_heaps);


	for (i = 0; i < num_heaps; i++) {
		heap = get_ulong(p_heaps + i*8);
		dump_one_heap(prefix, level, heap);
	}

	return 0;
}

/*
 *  Arguments are passed to the command functions in the global args[argcnt]
 *  array.  See getopt(3) for info on dash arguments.  Check out defs.h and
 *  other crash commands for usage of the myriad of utility routines available
 *  to accomplish what your task.
 */
void
cmd_ion(void)
{
	int c;
	dump_flags = 0;
	while ((c = getopt(argcnt, args, "abc")) != EOF) {
		switch(c)
		{
		case 'a':
			set_dump(DUMP_BUFFER);
			set_dump(DUMP_CLIENT);
			break;
		case 'b':
			set_dump(DUMP_BUFFER);
			break;
		case 'c':
			set_dump(DUMP_CLIENT);
			break;

		default:
			argerrs++;
			break;
		}
	}

	if (argerrs)
		cmd_usage(pc->curcmd, SYNOPSIS);

	dump_ion_heaps("\t", 0, 0);
}

/*
 *  The optional help data is simply an array of strings in a defined format.
 *  For example, the "help ion" command will use the help_ion[] string
 *  array below to create a help page that looks like this:
 *
 *    NAME
 *      ion - dump ion heaps information
 *
 *    SYNOPSIS
 *      ion arg ...
 *
 *    DESCRIPTION
 *      This command helps to dump ion heap information for ion leak problem
 *
 *    EXAMPLE
 *      ion
 *
 *        crash> ion
 *
 */
 
char *help_ion[] = {
	"ion",                         /* command name */
	"dump ion heap information",   /* short description */
	"[-a|-b|-c]",                  /* argument synopsis */
	"  This command dump ion heap information for debug ion heap leak",
	"",
	"        -a  displays all information of the ion heaps",
	"        -b  displays ion buffer information too.",
	"        -c  displays ion_client information of existing ion heaps.",
	"",
	"\nEXAMPLE",
	"  Echo back all command arguments:\n",
	"    crash> ion",
	"    num_heaps:7",
	"    ---------- ion heap ---------------",
	"    ion_heap:fffffff271d8d800",
	"    name:system",
	"    total_allocated:2054422528",
	"    total_handles:2054418432",
	"    ion_device:fffffff2718b9300",
	"    clients_node:fffffff2718b93c0",
	NULL
};


