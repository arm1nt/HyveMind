#ifndef _HYVEMIND_DRIVERS_UART16550_H
#define _HYVEMIND_DRIVERS_UART16550_H

int uart16650_initialize(void);

void uart16550_putc(char c);

void uart16550_puts(char *s);

#endif /* _HYVEMIND_DRIVERS_UART16550_H */

