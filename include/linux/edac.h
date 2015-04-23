/*
 * Generic EDAC defs
 *
 * Author: Dave Jiang <djiang@mvista.com>
 *
 * 2006-2008 (c) MontaVista Software, Inc. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 *
 */
#ifndef _LINUX_EDAC_H_
#define _LINUX_EDAC_H_

#include <linux/atomic.h>
#include <linux/device.h>

#define EDAC_OPSTATE_INVAL	-1
#define EDAC_OPSTATE_POLL	0
#define EDAC_OPSTATE_NMI	1
#define EDAC_OPSTATE_INT	2

extern int edac_op_state;
extern int edac_err_assert;
extern atomic_t edac_handlers;
extern struct bus_type edac_subsys;

extern int edac_handler_set(void);
extern void edac_atomic_assert_error(void);
extern struct bus_type *edac_get_sysfs_subsys(void);
extern void edac_put_sysfs_subsys(void);

enum {
	EDAC_REPORTING_ENABLED,
	EDAC_REPORTING_DISABLED,
	EDAC_REPORTING_FORCE
};

extern int edac_report_status;
#ifdef CONFIG_EDAC
static inline int get_edac_report_status(void)
{
	return edac_report_status;
}

static inline void set_edac_report_status(int new)
{
	edac_report_status = new;
}
#else
static inline int get_edac_report_status(void)
{
	return EDAC_REPORTING_DISABLED;
}

static inline void set_edac_report_status(int new)
{
}
#endif

static inline void opstate_init(void)
{
	switch (edac_op_state) {
	case EDAC_OPSTATE_POLL:
	case EDAC_OPSTATE_NMI:
		break;
	default:
		edac_op_state = EDAC_OPSTATE_POLL;
	}
	return;
}

#endif
