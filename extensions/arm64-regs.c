/* arm64-regs.c - crash extention for arm64 cpu core regs load
 *
 * Copyright (C) 2018 Schspa, Inc. All rights reserved.
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
#include<unistd.h>
#include <getopt.h>


static int
arm64_get_regs_before_stop(void)
{
	struct machine_specific *ms = machdep->machspec;
	ulong regs_before_stop_addr;
	ulong *notes_ptrs;
	ulong i;
	struct arm64_pt_regs *ptreg = NULL;

	if (!symbol_exists("regs_before_stop"))
		return FALSE;

	regs_before_stop_addr = symbol_value("regs_before_stop");

	if (regs_before_stop_addr == 0) {
		return FALSE;
	}

	notes_ptrs = (ulong *)GETBUF((kt->cpus - 1)*sizeof(notes_ptrs[0]));


	if (symbol_exists("__per_cpu_offset")) {
		/*
		 * Add __per_cpu_offset for each cpu to form the notes pointer.
		 */
		for (i = 0; i<kt->cpus; i++)
			notes_ptrs[i] = regs_before_stop_addr + kt->__per_cpu_offset[i];
	}

	if (!(ms->panic_task_regs = calloc((size_t)kt->cpus, sizeof(struct arm64_pt_regs)))) {
		error(FATAL, "cannot calloc panic_task_regs space\n");
		goto fail;
	}

	ptreg = ms->panic_task_regs;
	for  (i = 0; i < kt->cpus; i++) {
		if (!readmem(notes_ptrs[i], KVADDR, &ptreg[i], sizeof(struct arm64_pt_regs),
					 "pt_regs", RETURN_ON_ERROR)) {
			error(WARNING, "failed to read regs_before_stop\n");
			goto fail;
		}
	}

	FREEBUF(notes_ptrs);
	return TRUE;

fail:
	FREEBUF(notes_ptrs);
	free(ms->panic_task_regs);
	ms->panic_task_regs = NULL;
	return FALSE;
}

static void get_core_regs_from_dump_log(char *path)
{
	FILE *fp;
	char line[1024];
	u64 cpu = 0;
	struct machine_specific *ms = machdep->machspec;
	struct arm64_pt_regs *ptreg = NULL;

	printf("loading cpu core regs from %s\n", path);
	fp=fopen(path,"r");

	if(fp==NULL)
	{
		printf("can not load file!");
		return;
	}

	if (!ms->panic_task_regs) {
		if (!(ms->panic_task_regs = calloc((size_t)kt->cpus, sizeof(struct arm64_pt_regs)))) {
			error(FATAL, "cannot calloc panic_task_regs space\n");
			fclose(fp);
			return;
		}
	}
	ptreg = ms->panic_task_regs;

	while(!feof(fp))
	{
		int regnum;
		u64 value = 0;
		char *pname, *pvalue;
		char *str;
		char *ptr = fgets(line,1000,fp);
		if (ptr){
			while (*ptr == ' ' || *ptr == '\t') ptr++;
			if (*ptr == '#')
				continue;
			pname = strtok(ptr, "=");
			if (!pname)
				continue;
			pvalue = strtok(NULL, "=");
			value = strtoul(pvalue, &str, 16);
			if (!strncmp(pname, "cpu", 3)) {
				cpu = value;
				continue;//
			}
			if (cpu >= kt->cpus) {
				continue;
			}
			if (!strncmp(pname, "pc", 2)) {
				ptreg[cpu].pc = value;
			}
			else if (!strncmp(pname, "sp", 2)) {
				ptreg[cpu].sp = value;
			} else if (!strncmp(pname, "lr", 2)) {
				ptreg[cpu].regs[30] = value;
			}
			else if (*pname == 'x' || *pname == 'X'){
				pname++;
				regnum = atoi(pname);
				if (regnum >= 0 && regnum <= 30) {
					ptreg[cpu].regs[regnum] = value;
				}
			}
		}
	}
	fclose(fp);
	printf("loading cpu core regs from %s done\n", path);
}

static const struct option long_options[] = {
	{"load",1,NULL,'l'},
	{"atomic",1,NULL,'a'}
};

/*
 *  Arguments are passed to the command functions in the global args[argcnt]
 *  array.  See getopt(3) for info on dash arguments.  Check out defs.h and
 *  other crash commands for usage of the myriad of utility routines available
 *  to accomplish what your task.
 */
static void cmd_arm64_core_set(void)
{
	int opt;

	while((opt=getopt_long(argcnt, args,"l:a",long_options,NULL))!=-1)
	{
		//optarg is global
		switch(opt)
		{
		case 'l':
			get_core_regs_from_dump_log(optarg);
			break;
		case 'a':
			if (arm64_get_regs_before_stop()) {
				fprintf(fp, "find cpu core from ramdump success\n");
			} else {
				fprintf(fp, "find cpu core from ramdump failed\n");
			}
			break;
		default:
			printf("argument error %s\n", optarg);
			cmd_usage(pc->curcmd, SYNOPSIS);
			break;
		}
	}
}


char *help_arm64_core_set[] = {
	"arm64_core_set",                        /* command name */
	"set cpu core regs for arm64",   /* short description */
	"-l --load regvalue.txt",
	"-a --atomic //automatic load cpu core regs from dump",

	"  This command find arm64 cpu core reg from ramdump and set it.",
	"\narm64_core_set \n",
	"  set cpu core regs:\n",
	"    crash> arm64_core_set -a",
	"    find cpu core from ramdump success\n",
	NULL
};

static struct command_table_entry command_table[] = {
	{ "arm64_core_set", cmd_arm64_core_set, help_arm64_core_set, 0},          /* One or more commands, */
	{ NULL },                                     /* terminated by NULL, */
};

void __attribute__((constructor))
arm64_core_init(void) /* Register the command set. */
{
	register_extension(command_table);
}

void __attribute__((destructor))
arm64_core_fini(void) { }


