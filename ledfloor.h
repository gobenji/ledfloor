#ifndef _LEDFLOOR_H
#define _LEDFLOOR_H


// This contains GPIO pin numbers
struct ledfloor_config
{
	int ce;
	int a[11];
	int data[8];
};

/*
struct ledfloor_config pin_config = {
	.ce = GPIO_PIN_PA(0),
	.a = {
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
	},
	.data = {
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
		GPIO_PIN_PA(0),
	},
};
 */
#endif
