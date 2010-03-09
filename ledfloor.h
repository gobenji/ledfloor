#ifndef _LEDFLOOR_H
#define _LEDFLOOR_H

#include <linux/ioctl.h>

#define LFROWS 24
#define LFCOLS 48

#define LF_IOC_MAGIC 0x88
#define LF_IOCSLATCHNDELAY _IOW(LF_IOC_MAGIC, 0, unsigned int)
#define LF_IOCSCLKNDELAY _IOW(LF_IOC_MAGIC, 1, unsigned int)
#define LF_IOC_NB 2

#endif
