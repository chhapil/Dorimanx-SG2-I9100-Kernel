#ifndef __LINUX_GENERIC_NETLINK_H
#define __LINUX_GENERIC_NETLINK_H

#include <uapi/linux/genetlink.h>


/* All generic netlink requests are serialized by a global lock.  */
extern void genl_lock(void);
extern void genl_unlock(void);
#ifdef CONFIG_PROVE_LOCKING
extern int lockdep_genl_is_held(void);
#endif

#endif	/* __LINUX_GENERIC_NETLINK_H */
