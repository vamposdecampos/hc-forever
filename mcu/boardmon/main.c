#define F_CPU	14000000

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#define BAUD_RATE 57600

#define nop() do { __asm__ __volatile__ ("nop"); } while (0)
/* #define nop() _delay_ms(1) */

/******************************************************************************/
/* UART */

static int uart_putchar(char c, FILE *fp)
{
	if (c == '\n')
		uart_putchar('\r', fp);
	loop_until_bit_is_set(UCSRA, UDRE);
	UDR = c;
	return 0;
}

static int uart_getchar(FILE *fp)
{
	char res;

	(void) fp;

	loop_until_bit_is_set(UCSRA, RXC);
	res = UDR;
	uart_putchar(res, fp);	/* echo */
	return res;
}

FILE uart_fp = FDEV_SETUP_STREAM(uart_putchar, uart_getchar, _FDEV_SETUP_RW);

static void uart_init(void)
{
	UBRRH = 0;
	UBRRL = F_CPU / (BAUD_RATE * 16L) - 1;
	UCSRB = _BV(TXEN) | _BV(RXEN);
	UCSRC = _BV(URSEL) | _BV(UCSZ1) | _BV(UCSZ0);

	stdin = stdout = &uart_fp;
}

/******************************************************************************/

/*

pins:
PE2 - !BUSRQ
PE0 - !BUSACK
PE1 - ADDR_LE
PD4 - !RESET (CPU)

PD7 - !RD
PD6 - !WR

PA7..0 - CPU_A15..8
PC7..0 - D7..0

PD5 - temporary LED0

*/

#define BUSRQ_n		E, 2
#define BUSACK_n	E, 0
#define ADDR_LE		E, 1
#define RD_n		D, 7
#define WR_n		D, 6
#define RESET_n		D, 4
#define LED0		D, 5
#define CLKOUT		B, 0

#define ADDR_PORT	PORTA
#define ADDR_DDR	DDRA
#define ADDR_PIN	PINA
#define DATA_PORT	PORTC
#define DATA_DDR	DDRC
#define DATA_PIN	PINC

#define bit_set(r, b, v)	\
	do { \
		if (v) (r) |= _BV(b); \
		else (r) &= ~_BV(b); \
	} while (0)
#define bit_get(r, b)	(!!((r) & _BV(b)))

#define _output(port, b, v)	bit_set(PORT ## port, b, v)
#define _ddr(port, b, v)	bit_set(DDR ## port, b, v)
#define _input(port, b)		bit_get(PIN ## port, b)

#define output(signal, v)	_output(signal, v)
#define ddr(signal, v)		_ddr(signal, v)		/* v==1 -> output */
#define input(signal)		_input(signal)

#define pullup(signal)		do { _ddr(signal, 0); _output(signal, 1); } while (0)
#define pulldown(signal)	do { _output(signal, 0); _ddr(signal, 1); } while (0)

/******************************************************************************/

static void clockgen_init(void)
{
	/* set Timer0 to generate a 7MHz clock output on OC0 */
	TCCR0 = _BV(WGM01) | _BV(COM00) | _BV(CS00);
	OCR0 = 0;
	ddr(CLKOUT, 1);
}

/******************************************************************************/

int main(void)
{
	DDRA = 0;
	DDRB = 0;
	DDRC = 0;
	DDRD = 0;
	DDRE = 0;

	PORTA = 0xff;	
	PORTB = 0xff;
	PORTC = 0xff;
	PORTD = 0xff;
	PORTE = 0xff;

	ddr(LED0, 1);
	ddr(ADDR_LE, 1);
	pullup(BUSRQ_n);
	pulldown(RESET_n); /* XXX */

	/* this is helpful when using the bootloader */
	_delay_ms(100);

	uart_init();
	printf_P(PSTR("\n\nHC2006 (C) 2006-2009 by Vampire-\n"
		__DATE__ " " __TIME__ "\n\n"));

	output(LED0, 1);
	clockgen_init();

	printf_P(PSTR("booted\n"));
	while (1) {
		int ch;
		
		printf_P(PSTR("\nmon> "));
		ch = getchar();
		switch (ch) {
		/* dooo something. */
		default:
			break;
		}
		
		output(LED0, !input(LED0));
	}
	
	return 0;
}
