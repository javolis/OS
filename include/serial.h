/* serial.h — COM1 serial output (headless / CI logging). */
#pragma once

void serial_init(void);
void serial_putchar(char c);
void serial_write(const char *s);
