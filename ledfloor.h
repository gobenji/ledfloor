#ifndef _LEDFLOOR_H
#define _LEDFLOOR_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define LFROWS 24
#define LFCOLS 48

#define LF_IOC_MAGIC 0x88
#define LF_IOCSLATCHNDELAY _IOW(LF_IOC_MAGIC, 0, unsigned int)
#define LF_IOCSCLKNDELAY _IOW(LF_IOC_MAGIC, 1, unsigned int)
#define LF_IOCSGAMMATABLE _IOW(LF_IOC_MAGIC, 2, uint16_t[256])
#define LF_IOC_NB 3

struct command_t {
	__be32 latch_ndelay;
	__be32 clk_ndelay;
	float gamma;
	float contrast;
	float brightness;
	bool blank;
};

#endif
