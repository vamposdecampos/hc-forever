#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/sleep.h>

int main(void)
{
	wdt_enable(WDTO_1S);

	DDRA = 0;
	DDRB = 0;
	DDRC = 0;
	DDRD = _BV(5);
	DDRE = 0;

	PORTA = 0xff;	
	PORTB = 0xff;
	PORTC = 0xff;
	PORTD = 0xff;
	PORTE = 0xff;

//	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
//	sleep_mode();

	TCCR1B = 3; /* prescaler = 1/64 -> 1 count = 5.33333 us */


	while (1) {
		wdt_reset();

		if (TIFR & (1 << OCF1A)) {
			TIFR = 1 << OCF1A;
			PORTD ^= _BV(5);
		}
	}
	
	return 0;
}
