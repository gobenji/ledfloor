#!/usr/bin/python

def reverse12(a):
	b= 0
	for i in range(12):
		b<<= 1
		b|= a & 1
		a>>= 1
	
	return b

print ", ".join([str(reverse12(int(round((float(i) / 255)**(2.2) * 4095)))) for i in range(256)])
