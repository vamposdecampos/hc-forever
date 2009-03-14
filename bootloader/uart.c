#include <inttypes.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/signal.h>
#include <avr/boot.h>
#include <setjmp.h>

#define BAUD 48//00
#define CRYSTAL 1//MHz
#define BAUD_SETTING ((CRYSTAL*10000)/(16*BAUD)-1)

#if SPM_PAGESIZE > 128
	#define DATA_BUFFER_SIZE SPM_PAGESIZE
#else
	#define DATA_BUFFER_SIZE SPM_PAGESIZE
#endif

#define XMODEM_NUL 0x00
#define XMODEM_SOH 0x01
#define XMODEM_STX 0x02
#define XMODEM_EOT 0x04
#define XMODEM_ACK 0x06
#define XMODEM_NAK 0x15
#define XMODEM_CAN 0x18
#define XMODEM_EOF 0x1A

#define XMODEM_RECIEVING_WAIT_CHAR 	'C'

void (*app)(void)=0;
void initIo(uint16_t baud)
{

	/* Set baud rate */
	UBRRH = (unsigned char)(baud>>8);
	UBRRL = (unsigned char)baud;
	
	/* Enable Receiver and Transmitter */
	UCSRB = (1<<RXEN)|(1<<TXEN);
	/* Set frame format: 8data, 2stop bit */
	UCSRC = (1<<URSEL)|(1<<USBS)|(3<<UCSZ0);
	//INIT THE TIMER0 AS THE FREE RUN FOR AUTO 
	TIMSK=TIMSK&(~(1<<TOIE0));//MASK THE TIMER0 OVERFLOW INTERRUPT
    TCNT0=0;//SET THE TIMER0 COUNT TO ZERO
    TCCR0 = 1<<CS00;//set the source clk for timer0 as the main clock
}

int
uart_putchar(char c)
{
	loop_until_bit_is_set(UCSRA, UDRE);
	UDR = c;
	return 0;
}

int uart_getchar(void)
{
	unsigned char status, resh, resl;
	/* no data to be received */
	if( !(UCSRA & (1<<RXC)) )
	return -1;
	/* Get status and ninth bit, then data */
	/* from buffer */
	status = UCSRA;
	resh = UCSRB;
	resl = UDR;
	/* If error, return -1 */
	if ( status & ((1<<FE)|(1<<DOR)|(1<<PE)))
	return -1;
	/* Filter the ninth bit, then return */
	//resh = (resh >> 1) & 0x01;
	return resl;
}

int uart_waitchar(void)
{
	int c;
	while((c=uart_getchar())==-1);
	return c;
}

int calcrc(char *ptr, int count)
{
	int crc;
	char i;
	crc = 0;
	while (--count >= 0)
	{
		crc = crc ^ (int) *ptr++ << 8;
		i = 8;
		do
		{
		if (crc & 0x8000)
			crc = crc << 1 ^ 0x1021;
		else
			crc = crc << 1;
		} while(--i);
	}
	return (crc);
}
const char startupString[]="press key 'd' to download,press other key to execute the application\n\r\0";

int main(void)
{
	int i,j;
	unsigned char timercount=0;
	unsigned char packNO;
	unsigned long address;
	unsigned long bufferPoint;
	unsigned char data[DATA_BUFFER_SIZE];
	unsigned int crc;
	initIo(BAUD_SETTING);
	//fdevopen(uart_putchar,uart_getchar,0);
	i=0;
	while(startupString[i]!='\0')
	{
		uart_putchar(startupString[i]);
		i++;
	}
	if(uart_waitchar()!='d')app();
	while(uart_getchar()!=XMODEM_SOH)
	{
	if(TIFR&(1<<TOV0))
		{
			if(timercount==200)
			{
				uart_putchar(XMODEM_RECIEVING_WAIT_CHAR);
				timercount=0;
			}
			timercount++;
			TIFR=TIFR|(1<<TOV0);
		}
	}
	packNO=1;
	address=0;
	bufferPoint=0;
	do
	{
		if(packNO==(char)uart_waitchar())
		{
			if(packNO==(unsigned char)(~uart_waitchar()))
			{
				for(i=0;i<128;i++)
				{
					data[bufferPoint]=(unsigned char)uart_waitchar();	
					bufferPoint++;	
				}
				crc=0;
				crc+=(uart_waitchar()<<8);
				crc+=uart_waitchar();
				if(calcrc(&data[bufferPoint-128],128)==crc)
				{
					while(bufferPoint>=SPM_PAGESIZE)
					{
						
						boot_page_erase(address);
						while(boot_rww_busy())
					    {
	            			boot_rww_enable();
	        			}
						for(i=0;i<SPM_PAGESIZE;i+=2)
	            		{
	            			boot_page_fill(address%SPM_PAGESIZE,data[i]+(data[i+1]<<8));
	      					address+=2;
	      				}	
	        			boot_page_write(address-1);
	        			while(boot_rww_busy())
	        			{
	            			boot_rww_enable();
	        			}
						for(j=0;i<bufferPoint;i++,j++)
						{
							data[j]=data[i];
						}
						bufferPoint=j;
					}	
					uart_putchar(XMODEM_ACK);
					packNO++;
				}
				else
				{
					uart_putchar(XMODEM_NAK);
				}
			}
		}
		else
		{
			uart_putchar(XMODEM_NAK);
		}
	}while(uart_waitchar()!=XMODEM_EOT);
	uart_putchar(XMODEM_ACK);
	(app)();
}



