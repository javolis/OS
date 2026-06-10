/* serial.h — COM1 serial port output (used for headless / CI logging). */
#pragma once

void serial_init(void);
void serial_putchar(char c);
void serial_write(const char *s);
