/* power.h — machine power control (ACPI shutdown + 8042 reset). */
#pragma once

void power_off(void) __attribute__((noreturn));
void power_reboot(void) __attribute__((noreturn));
