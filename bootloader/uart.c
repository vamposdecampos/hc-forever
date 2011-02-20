#define F_CPU	14000000

#include <inttypes.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/boot.h>
#include <avr/delay.h>
#include <setjmp.h>

#define BAUD 57600
#define BAUD_SETTING (F_CPU/(16L * BAUD)-1)

#if SPM_PAGESIZE > 128
#define DATA_BUFFER_SIZE SPM_PAGESIZE
#else
#define DATA_BUFFER_SIZE 128
#endif

#define XMODEM_NUL 0x00
#define XMODEM_SOH 0x01
#define XMODEM_STX 0x02
#define XMODEM_EOT 0x04
#define XMODEM_ACK 0x06
#define XMODEM_NAK 0x15
#define XMODEM_CAN 0x18
#define XMODEM_EOF 0x1A

#define XMODEM_RECEIVING_WAIT_CHAR 	'C'

void (*app) (void) = 0;
void initIo(uint16_t baud)
{

	/* Set baud rate */
	UBRRH = (unsigned char) (baud >> 8);
	UBRRL = (unsigned char) baud;

	/* Enable Receiver and Transmitter */
	UCSRB = (1 << RXEN) | (1 << TXEN);
	/* Set frame format: 8data, 2stop bit */
	UCSRC = (1 << URSEL) | (1 << USBS) | (3 << UCSZ0);
	//INIT THE TIMER0 AS THE FREE RUN FOR AUTO 
	TIMSK = TIMSK & (~(1 << TOIE0));	//MASK THE TIMER0 OVERFLOW INTERRUPT
	TCNT0 = 0;		//SET THE TIMER0 COUNT TO ZERO
	TCCR0 = 1 << CS00;	//set the source clk for timer0 as the main clock
}

int uart_putchar(char c)
{
	loop_until_bit_is_set(UCSRA, UDRE);
	UDR = c;
	return 0;
}

static void uart_putstr(const char *s)
{
	while (*s)
		uart_putchar(*s++);
}

int uart_getchar(void)
{
	unsigned char status, resh, resl;
	/* no data to be received */
	if (!(UCSRA & (1 << RXC)))
		return -1;
	/* Get status and ninth bit, then data */
	/* from buffer */
	status = UCSRA;
	resh = UCSRB;
	resl = UDR;
	/* If error, return -1 */
	if (status & ((1 << FE) | (1 << DOR) | (1 << PE)))
		return -1;
	/* Filter the ninth bit, then return */
	//resh = (resh >> 1) & 0x01;
	return resl;
}

int uart_waitchar(void)
{
	int c;
	while ((c = uart_getchar()) == -1);
	return c;
}

uint16_t calcrc(unsigned char *ptr, int count)
{
	int crc;
	char i;
	crc = 0;
	while (--count >= 0) {
		crc = crc ^ (int) *ptr++ << 8;
		i = 8;
		do {
			if (crc & 0x8000)
				crc = crc << 1 ^ 0x1021;
			else
				crc = crc << 1;
		} while (--i);
	}
	return (crc);
}

const char startupString[] = "\r\n"
	"HC2k bootloader " __DATE__ " " __TIME__ "\r\n"
	"[D]ownload or [R]un.\r\n";

const char runString[] = "\r\nRun.\r\n";
const char cancelStr[] = {XMODEM_CAN, XMODEM_CAN, XMODEM_CAN, XMODEM_CAN, XMODEM_CAN, 0};

int main(void)
{
	int i, j;
	unsigned short timercount = 0;
	unsigned char packNO;
	unsigned long address;
	unsigned long bufferPoint;
	unsigned char data[DATA_BUFFER_SIZE];
	uint16_t crc;

	initIo(BAUD_SETTING);
top:
	uart_putstr(startupString);
	for (i = 0; i < 30; i++) {
		int ch = uart_getchar();
		if (ch == 'r')
			break;
		if (ch == 'd')
			goto download;
		_delay_ms(100);
		uart_putchar('.');
	}
	goto run;

download:
	while (uart_getchar() != XMODEM_SOH) {
		if (TIFR & (1 << TOV0)) {
			if (timercount == 5000) {
				uart_putchar(XMODEM_RECEIVING_WAIT_CHAR);
				timercount = 0;
			}
			timercount++;
			TIFR = TIFR | (1 << TOV0);
		}
	}
	packNO = 1;
	address = 0;
	bufferPoint = 0;
	do {
		if (packNO == (unsigned char) uart_waitchar()) {
			if (packNO == (unsigned char) (~uart_waitchar())) {
				for (i = 0; i < 128; i++) {
					data[bufferPoint] = (unsigned char) uart_waitchar();
					bufferPoint++;
				}
				crc = uart_waitchar() << 8;
				crc |= uart_waitchar();
				if (calcrc(&data[bufferPoint - 128], 128) == crc) {
					if (address >= 0x1c00) {
						/* don't allow overwriting the bootloader */
						uart_putstr(cancelStr);
						goto top;
					}
					while (bufferPoint >= SPM_PAGESIZE) {
						boot_page_erase(address);
						while (boot_rww_busy()) {
							boot_rww_enable();
						}
						for (i = 0; i < SPM_PAGESIZE; i += 2) {
							boot_page_fill(address % SPM_PAGESIZE,
								       data[i] + (data[i + 1] << 8));
							address += 2;
						}
						boot_page_write(address - 1);
						while (boot_rww_busy()) {
							boot_rww_enable();
						}
						for (j = 0; i < bufferPoint; i++, j++) {
							data[j] = data[i];
						}
						bufferPoint = j;
					}
					uart_putchar(XMODEM_ACK);
					packNO++;
				} else {
					uart_putchar(XMODEM_NAK);
				}
			}
		} else {
			uart_putchar(XMODEM_NAK);
		}
	} while (uart_waitchar() != XMODEM_EOT);
	uart_putchar(XMODEM_ACK);

run:
	uart_putstr(runString);
	(app) ();
}
