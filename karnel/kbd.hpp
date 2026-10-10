#pragma once
void kbd_init();      // routes IRQ1 and installs the handler
int  kbd_pop();       // next character or -1
