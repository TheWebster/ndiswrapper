/*
 *  Copyright (C) 2003-2005 Pontus Fuchs, Giridhar Pemmasani
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 */

#include "ndis.h"
#include "iw_ndis.h"
#include "loader.h"
#include "pnp.h"
#include "wrapper.h"

char *if_name = "wlan%d";
int proc_uid, proc_gid;
int hangcheck_interval;
static char *utils_version = UTILS_VERSION;
int debug = DEBUG;

module_param(if_name, charp, 0400);
MODULE_PARM_DESC(if_name, "Network interface name or template "
		 "(default: wlan%d)");
module_param(proc_uid, int, 0600);
MODULE_PARM_DESC(proc_uid, "The uid of the files created in /proc "
		 "(default: 0).");
module_param(proc_gid, int, 0600);
MODULE_PARM_DESC(proc_gid, "The gid of the files created in /proc "
		 "(default: 0).");
module_param(debug, int, 0600);
MODULE_PARM_DESC(debug, "debug level");

/* 0 - default value provided by NDIS driver,
 * positive value - force hangcheck interval to that many seconds
 * negative value - disable hangcheck
 */
module_param(hangcheck_interval, int, 0600);
MODULE_PARM_DESC(hangcheck_interval, "The interval, in seconds, for checking"
		 " if driver is hung. (default: 0)");

module_param(utils_version, charp, 0400);
MODULE_PARM_DESC(utils_version, "Compatible version of utils "
		 "(read only: " UTILS_VERSION ")");

MODULE_AUTHOR("ndiswrapper team <ndiswrapper-general@lists.sourceforge.net>");
#ifdef MODULE_DESCRIPTION
MODULE_DESCRIPTION("NDIS wrapper driver");
#endif
#ifdef MODULE_VERSION
MODULE_VERSION(DRIVER_VERSION);
#endif
MODULE_LICENSE("GPL");

static void module_cleanup(void)
{
	loader_exit();
	usb_exit();
	wrap_procfs_remove();
	wrapndis_exit();
	ndis_exit();
	ntoskernel_exit();
	wrapmem_exit();
}

static int __init wrapper_init(void)
{
#ifdef TAINT_OOT_MODULE
	add_taint(TAINT_OOT_MODULE, LOCKDEP_STILL_OK);
#endif
	printk(KERN_INFO "%s version %s loaded (smp=%s, preempt=%s)\n",
	       DRIVER_NAME, DRIVER_VERSION,
#ifdef CONFIG_SMP
	       "yes"
#else
	       "no"
#endif
		,
#ifdef CONFIG_PREEMPT_RT
		"rt"
#elif defined(CONFIG_PREEMPT)
		"yes"
#else
		"no"
#endif
		);

	if (wrapmem_init() || ntoskernel_init() || ndis_init() ||
	    wrapndis_init() || usb_init() || wrap_procfs_init() ||
	    loader_init()) {
		module_cleanup();
		ERROR("%s: initialization failed", DRIVER_NAME);
		return -EINVAL;
	}
	EXIT1(return 0);
}

static void __exit wrapper_exit(void)
{
	ENTER1("");
	module_cleanup();
}

module_init(wrapper_init);
module_exit(wrapper_exit);
