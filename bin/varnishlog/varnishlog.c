/*-
 * Copyright (c) 2006 Verdens Gang AS
 * Copyright (c) 2006-2015 Varnish Software AS
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@phk.freebsd.dk>
 * Author: Martin Blix Grydeland <martin@varnish-software.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Log tailer for Varnish
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/stat.h>

#include "vapi/vsm.h"
#include "vapi/vsl.h"
#include "vapi/voptget.h"
#include "vas.h"
#include "vdef.h"
#include "vpf.h"
#include "vut.h"
#include "miniobj.h"

static const char progname[] = "varnishlog";

static struct log {
	/* Options */
	int		a_opt;
	int		A_opt;
	char		*w_arg;

	/* State */
	FILE		*fo;
} LOG;

static void __attribute__((__noreturn__))
usage(int status)
{
	const char **opt;
	fprintf(stderr, "Usage: %s <options>\n\n", progname);
	fprintf(stderr, "Options:\n");
	for (opt = vopt_spec.vopt_usage; *opt != NULL; opt += 2)
		fprintf(stderr, " %-25s %s\n", *opt, *(opt + 1));
	exit(status);
}

static void
openout(int append)
{

	AN(LOG.w_arg);
	if (LOG.A_opt)
		LOG.fo = fopen(LOG.w_arg, append ? "a" : "w");
	else
		LOG.fo = VSL_WriteOpen(VUT.vsl, LOG.w_arg, append, 0);
	if (LOG.fo == NULL)
		VUT_Error(2, "Cannot open output file (%s)",
		    LOG.A_opt ? strerror(errno) : VSL_Error(VUT.vsl));
	VUT.dispatch_priv = LOG.fo;
}

static int __match_proto__(VUT_cb_f)
rotateout(void)
{

	AN(LOG.w_arg);
	AN(LOG.fo);
	fclose(LOG.fo);
	openout(1);
	AN(LOG.fo);
	return (0);
}

static int __match_proto__(VUT_cb_f)
flushout(void)
{

	AN(LOG.fo);
	if (fflush(LOG.fo))
		return (-5);
	return (0);
}

static int __match_proto__(VUT_cb_f)
sighup(void)
{
	return (1);
}

int
main(int argc, char * const *argv)
{
	int opt;

	VUT_Init(progname, argc, argv, &vopt_spec);
	memset(&LOG, 0, sizeof LOG);

	while ((opt = getopt(argc, argv, vopt_spec.vopt_optstring)) != -1) {
		switch (opt) {
		case 'a':
			/* Append to file */
			LOG.a_opt = 1;
			break;
		case 'A':
			/* Text output */
			LOG.A_opt = 1;
			break;
		case 'h':
			/* Usage help */
			usage(0);
		case 'w':
			/* Write to file */
			REPLACE(LOG.w_arg, optarg);
			break;
		default:
			if (!VUT_Arg(opt, optarg))
				usage(1);
		}
	}

	if (optind != argc)
		usage(1);

	if (VUT.D_opt && !LOG.w_arg)
		VUT_Error(1, "Missing -w option");

	/* Setup output */
	if (LOG.A_opt || !LOG.w_arg)
		VUT.dispatch_f = VSL_PrintTransactions;
	else
		VUT.dispatch_f = VSL_WriteTransactions;
	VUT.sighup_f = sighup;
	if (LOG.w_arg) {
		openout(LOG.a_opt);
		AN(LOG.fo);
		if (VUT.D_opt)
			VUT.sighup_f = rotateout;
	} else
		LOG.fo = stdout;
	VUT.idle_f = flushout;

	VUT_Setup();
	VUT_Main();
	VUT_Fini();

	(void)flushout();

	exit(0);
}
