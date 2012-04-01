#include <avr/io.h>
#include <inttypes.h>
#include "lcd_lib_4bit.h"
#include <util/delay.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#include "usbdrv.h"

// Macros
#define set_low(port,pin) port &= ~(1<<pin)
#define set_high(port,pin) port |= (1<<pin)
#define set_input(portdir,pin) portdir &= ~(1<<pin)
#define set_output(portdir,pin) portdir |= (1<<pin)

// Used for ongoing transmissions
uint8_t buff[16];				// Build transmitted strings here
uint8_t addr;

// Predefined messages:  "LCD Ready!" and "USB Ready!"
uint8_t lcdinit[16] = {76, 67, 68, 0, 82, 101, 97, 100, 121, 33, 0, 0, 0, 0, 0, 0};
uint8_t usbinit[16] = {85, 83, 66, 0, 82, 101, 97, 100, 121, 33, 0, 0, 0, 0, 0, 0};

int main(void)
{
	// Lights, backlights, and fan initialization
	set_output(DDRD,7);
	set_output(DDRD,6);
	set_output(DDRD,5);
	set_high(PORTD,7);

	TCCR0A = 0xA3;			// Timer0, channels A and B to Fast PWM Mode
	TCCR0B = 0x05;			// Prescaler is 1024 to give MOSFETS plenty of time
	OCR0B = 0xFF;			// Rev up Fan 1 for 2s to avoid stalling
	_delay_ms(2000);
	OCR0B = 0x7F;			// Reduce Fan 1 to 50% power (127)
	uchar i;
	for (i=0;i<3;i++){		// Blink lights to signal life
		OCR0A = 0xFF;
		_delay_ms(200);
		OCR0A = 0x00;
		_delay_ms(200);
	}
	OCR0A = 0x14;			// Init lights at low brightness (20)


	// LCD initialization
	_delay_ms(200);
	CHA_LCDinit();
	CHA_LCDclr();
	CHB_LCDinit();
	CHB_LCDclr();
	CHA_LCDstringLine(lcdinit,0);	// LCD ready to go!

	// USB initialization
    wdt_enable(WDTO_1S); 		// Enable 1s watchdog timer
    usbInit();
    usbDeviceDisconnect(); 		// Enforce re-enumeration
    for(i = 0; i<200; i++) { 	// Wait 200 ms
        wdt_reset();			// Keep the watchdog happy
        _delay_ms(1);
    }
    usbDeviceConnect();
    sei(); 							// Enable interrupts after re-enumeration
    CHB_LCDstringLine(usbinit,0);	// USB ready to go!

    while(1) {					// Feed watchdog and poll USB forever
        wdt_reset();
        usbPoll();
    }

	return 0;
}

// Automagically called upon receiving a USB control message
USB_PUBLIC uchar usbFunctionSetup(uchar data[8]) {
	// Clean up the data we received
    usbRequest_t *rq = (void *)data;
	uchar outputID;
	int cmdID;
	outputID = (rq->bRequest);
	cmdID = (rq->wValue.bytes[0]);

	// Lighting and fan related commands
	if (outputID==0)				// 0: Turn backlights off
		set_low(PORTD,7);
	else if(outputID==1)			// 1: Turn backlights on
		set_high(PORTD,7);
	else if(outputID==2){			// 2: Clear both displays
		CHA_LCDclr();
		CHB_LCDclr();
	}
	else if(outputID==3){			// 3: Adjust fan 1 speed
		if(cmdID==0)
			TCCR0A = TCCR0A&0xDF;	// Turn off completely if we got a zero
		else{
			OCR0B = cmdID;			// Take PWM value
			TCCR0A = TCCR0A|0x20;	// Make sure this channel PWM is on
		}
	}
	else if(outputID==5){			// 5: Adjust light speed
		if(cmdID==0)
			TCCR0A = TCCR0A&0x7F;	// Turn off completely if we got a zero
		else{
			OCR0A = cmdID;			// Take PWM value
			TCCR0A = TCCR0A|0x80;	// Make sure this channel PWM is on
		}
	}

	// LCD-related commands
	else if(outputID==6)			// 6: Post buffer to LCD A, line 0
		CHA_LCDstringLine(buff,0);
	else if(outputID==7)			// 7: Post buffer to LCD A, line 1
		CHA_LCDstringLine(buff,1);
	else if(outputID==8)			// 8: Post buffer to LCD B, line 0
		CHB_LCDstringLine(buff,0);
	else if (outputID==9)			// 9: Post buffer to LCD B, line 1
		CHB_LCDstringLine(buff,1);
	else if(outputID==10)			// 10: String incoming. Reset buffer index.
		addr = 0;
	else if(outputID==11){			// 11: Got a char. Store it in the buffer.
		buff[addr] = cmdID;
		addr++;
	}

	return 0;
}
