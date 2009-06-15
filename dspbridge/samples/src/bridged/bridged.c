/*
 * Bridge driver Daemon
 * DSP Recovery feature for TI OMAP processors.
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PROGRAM IS PROVIDED ''AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dbdefs.h>
#include <errbase.h>
#include <DSPManager.h>
#include <DSPProcessor.h>
#include <DSPProcessor_OEM.h>

#define try_err_out(msg, err)						\
do {									\
    if (DSP_FAILED(err)) {						\
	printf("%s failed : Err Num = %lx\n", msg, err);		\
	goto out;							\
    } else								\
	printf("%s succeeded\n", msg);					\
} while (0);

/* Recovery options */
#define RELOAD		0
#define REINSTALL	1

#define EVENTS		4
#define BASEIMAGE_FILE	"/system/lib/dsp/baseimage.dof"

char *evt_name[5] = {"MMU_FAULT", "SYS_ERROR", "PWR_ERROR",
		 "STATE_CHANGE", "UNKNOWN"};

unsigned long daemon_attach(DSP_HPROCESSOR *proc);
unsigned long daemon_detach(DSP_HPROCESSOR proc);
unsigned long bridge_listener(void);
unsigned long handle_event_action(unsigned int index);
void handle_error_state(DSP_HPROCESSOR proc);
unsigned long dsp_recovery(int recovtype, DSP_HPROCESSOR proc);
unsigned long load_baseimage(DSP_HPROCESSOR proc);
extern int pmanager(void);

int main ()
{
	pid_t child_pid, child_sid;

	/* Fork off the parent process */
	child_pid = fork();
	if (child_pid < 0) {
		exit(1); 	/* Failure */
	}
	/* If we got a good PID, then we can exit the parent process. */
	if (child_pid > 0) {
		exit(0);	/* Succeess */
	}
	/* Create a new SID for the child process */
	child_sid = setsid();
	if (child_sid < 0)
		exit(0);

	/* Close standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	bridge_listener();

	return 0;
}

unsigned long daemon_attach(DSP_HPROCESSOR *proc)
{
	unsigned long status;
	unsigned int index = 0;
	unsigned int prid = 0;
	unsigned int numprocs = 0;
	struct DSP_PROCESSORINFO dspinfo;

	status = DspManager_Open(0, NULL);
	try_err_out("DspManager_Open", status);

	/* Attach to DSP */
	do {
		status = DSPManager_EnumProcessorInfo(index, &dspinfo,
				 (unsigned int)sizeof(struct DSP_PROCESSORINFO),
				 &numprocs);

		if ((dspinfo.uProcessorType == DSPTYPE_55) ||
				 (dspinfo.uProcessorType == DSPTYPE_64)) {
			prid = index;
			status = DSP_SOK;
			break;
		}
		index++;
	} while (DSP_SUCCEEDED(status));
	try_err_out("DSP device detected", status);

	status = DSPProcessor_Attach(prid, NULL, proc);
	try_err_out("DSPProcessor_Attach", status);
out:
	return status;
}

unsigned long daemon_detach(DSP_HPROCESSOR proc)
{
	unsigned long status;

	status = DSPProcessor_Detach(proc);
	try_err_out("DSPProcessor_Detach", status);

	status = DspManager_Close(0, NULL);
	try_err_out("DspManager_Close", status);
out:
	return status;
}

unsigned long bridge_listener(void)
{
	DSP_HPROCESSOR proc;
	unsigned int index = 0, i;
	unsigned long status = DSP_SOK;
	struct DSP_NOTIFICATION *notifier[EVENTS];

	for (i = 0; i <= EVENTS; i++) {
		notifier[i] = malloc(sizeof(struct DSP_NOTIFICATION));
		if (!notifier[i])
			return DSP_EMEMORY;
		memset(notifier[i], 0, sizeof(struct DSP_NOTIFICATION));
	}


	/* Big listener loop */
	while (1) {
		proc = NULL;

		daemon_attach(&proc);

		/* Recover based on processor state = ERROR */
		handle_error_state(proc);

		/* Register notify objects */
		status = DSPProcessor_RegisterNotify(proc, DSP_MMUFAULT,
					DSP_SIGNALEVENT, notifier[0]);
		try_err_out("DSP node register notify DSP_MMUFAULT", status);

		status = DSPProcessor_RegisterNotify(proc, DSP_SYSERROR,
					DSP_SIGNALEVENT, notifier[1]);
		try_err_out("DSP node register notify DSP_SYSERROR", status);

		status = DSPProcessor_RegisterNotify(proc, DSP_PWRERROR,
					DSP_SIGNALEVENT, notifier[2]);
		try_err_out("DSP node register notify DSP_PWRERROR", status);

		status = DSPProcessor_RegisterNotify(proc,
			DSP_PROCESSORSTATECHANGE, DSP_SIGNALEVENT, notifier[3]);
		try_err_out("DSP node register notify DSP_StateChange", status);

		status = DSPManager_WaitForEvents(notifier, 4, &index,
								DSP_FOREVER);
		try_err_out("Catch notification signal", status);

		daemon_detach(proc);

		handle_event_action(index);
	}

out:
	daemon_detach(proc);

	for (i = 0; i <= EVENTS; i++)
		free(notifier[i]);

	return status;
}

unsigned long handle_event_action(unsigned int index)
{
	unsigned long status = DSP_EFAIL;
	DSP_HPROCESSOR proc = NULL;

	printf("bridged: DSP (%s) Crash detected, trying to recover...\n",
						 evt_name[index]);

	switch (index) {
	case 0:
		/* fall through */
	case 1:
		/* fall through */
	case 2:
		status = pmanager();
		if (status > 0) {
			daemon_attach(&proc);
			status = dsp_recovery(RELOAD, proc);
			daemon_detach(proc);
			try_err_out("Recover from DSP error", status);
		}
		break;
	case 3:
		daemon_attach(&proc);
		handle_error_state(proc);
		daemon_detach(proc);
		break;
	default:
		status = DSP_EFAIL;
		try_err_out("Catch unknown event, ", status);
		break;
	}
out:
	return status;
}

void handle_error_state(DSP_HPROCESSOR proc)
{
	unsigned int maxretries = 5;
	struct DSP_PROCESSORSTATE procstatus;

        do {
                DSPProcessor_GetState(proc, &procstatus, sizeof(procstatus));
                printf("bridged: DSP Processor state = %d\n", procstatus.iState);

                /* Exit loop if we are not in error state */
                if (procstatus.iState != PROC_ERROR)
                        break;

                printf("bridged: DSP Processor state = ERROR, reloading baseimage...\n");
                dsp_recovery(RELOAD, proc);
        } while (procstatus.iState == PROC_ERROR && maxretries-- > 0);
}

unsigned long dsp_recovery(int recovtype, DSP_HPROCESSOR proc)
{
	unsigned long status = DSP_EFAIL;

	switch(recovtype) {
	case RELOAD:
		status = load_baseimage(proc);
		break;
	case REINSTALL:
		/* not implemented */
		break;
	default:
		try_err_out("Recovery option not supported,", status);
		break;
	}
out:
	return status;
}

unsigned long load_baseimage(DSP_HPROCESSOR proc)
{
	unsigned long status;
	const char* argv[2] = {BASEIMAGE_FILE, NULL};

	status = DSPProcessor_Stop(proc);
	try_err_out("Stopping the DSPProcessor", status);

	status = DSPProcessor_Load(proc, 1, (const char **)argv, NULL);
	try_err_out("Load baseimage", status);

	status = DSPProcessor_Start(proc);
	try_err_out("Starting DSPProcessor", status);
out:
	return status;
}

