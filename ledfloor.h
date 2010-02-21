#ifndef _LEDFLOOR_H
#define _LEDFLOOR_H

#define LFROWS 24
#define LFCOLS 48

/* Use a linear buffer index to convert an RGB buffer into a ledfloor buffer
 * row= index % rows
 * col= cols - 1 - index / (rows * 3)
 * comp= 2 - index / rows % 3
 * rgb index= row * cols * 3 + col * 3 + comp
 */
#define CCR_TO_RCC(_index) \
	((_index % LFROWS) * LFCOLS * 3 + \
	(LFCOLS - 1 - _index / (LFROWS * 3)) * 3 + \
	(2 - _index / LFROWS % 3))

#endif
