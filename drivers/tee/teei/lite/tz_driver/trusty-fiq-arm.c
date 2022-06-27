// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2013 Google, Inc.
 *
 */

#include <asm/fiq_glue.h>
#include <linux/platform_device.h>
#include <teei_trusty.h>
#include <teei_smcall.h>

#include "trusty-fiq.h"

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

static void __naked trusty_fiq_return(void)
{
	asm volatile(
		".arch_extension sec\n"
		"mov	r12, r0\n"
		"ldr	r0, =" STRINGIFY(SMC_FC_FIQ_EXIT) "\n"
		"smc	#0");
}

int trusty_fiq_arch_probe(struct platform_device *pdev)
{
	return fiq_glue_set_return_handler(trusty_fiq_return);
}

void trusty_fiq_arch_remove(struct platform_device *pdev)
{
	fiq_glue_clear_return_handler(trusty_fiq_return);
}
