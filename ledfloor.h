#ifndef _LEDFLOOR_H
#define _LEDFLOOR_H


/* This contains GPIO pin numbers
 * The structure must match what is in arch/avr32/boards/atngw100/setup.c */
struct ledfloor_config
{
	int ce;
	int a[12];
	int data[8];
};

#endif
