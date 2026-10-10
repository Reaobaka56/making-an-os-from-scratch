#pragma once
void serial_init();
void serial_putc(char c);
int  serial_getc_nb();   // -1 if no byte waiting
