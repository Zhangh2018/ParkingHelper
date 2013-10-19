/*
 * ParkingHelper.cpp
 *
 * Created: 10/14/2013 2:29:40 PM
 *  Author: Matthew
 */ 

#include <avr/io.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "LPD8806tiny.h"

const uint8_t NUM_LEDS = 4;
const uint32_t TIMEOUT_CYCLES = F_CPU / 1000 * 20;
const uint8_t MICROSECONDS_PER_TICK = 8;
const uint16_t cautionDistanceCm = 150;

volatile uint16_t echoTicks = 0;

LPD8806 leds(NUM_LEDS, PB0 /* data */, PB2 /* clock */);

void setAllLedsToColor(const Color& color)
{
	for (uint16_t i = 0; i < leds.numPixels(); ++i)
	{
		leds.setPixelColor(i, color);
	}
}

uint16_t readDistance()
{
	uint32_t timeout;
	
	// Trigger distance reading
	DDRB |= (1 << DDB1);	// Set as output
	PORTB |= (1 << PB1);	// Set HIGH
	_delay_us(3);
	PORTB &= ~(1 << PB1);	// Set LOW
	DDRB &= ~(1 << DDB1);	// Set as input
	_delay_us(500);
		
	ATOMIC_BLOCK(ATOMIC_FORCEON)
	{
		echoTicks = 0;
	}
		
	// Wait for distance sensor signal to go high or timeout
	timeout = TIMEOUT_CYCLES;	
	while (!(PINB & (1 << PINB1)))
	{
		if (--timeout == 0)
		{
			return 0;
		}
	}
		
	// Wait for distance sensor signal to return low or timeout
	timeout = TIMEOUT_CYCLES;
	while (PINB & (1 << PINB1))
	{
		if (--timeout == 0)
		{
			return 0;
		}
	}
		
	uint16_t ticks;
	ATOMIC_BLOCK(ATOMIC_FORCEON)
	{
		ticks = echoTicks;		
	}
	
	/*
	Color c0 = Color( ((ticks >> 11) & 0x1) * 127, ((ticks >> 10) & 0x1) * 127, ((ticks >> 9) & 0x1) * 127);
	Color c1 = Color( ((ticks >> 8) & 0x1) * 127, ((ticks >> 7) & 0x1) * 127, ((ticks >> 6) & 0x1) * 127);
	Color c2 = Color( ((ticks >> 5) & 0x1) * 127, ((ticks >> 4) & 0x1) * 127, ((ticks >> 3) & 0x1) * 127);
	Color c3 = Color( ((ticks >> 2) & 0x1) * 127, ((ticks >> 1) & 0x1) * 127, ((ticks >> 0) & 0x1) * 127);
	leds.setPixelColor(0, c0);
	leds.setPixelColor(1, c1);
	leds.setPixelColor(2, c2);
	leds.setPixelColor(3, c3);
	leds.show();
	*/
	
	// return ticks * ((float)MICROSECONDS_PER_TICK * 34029.0f / 1000000.0f);
	return ticks;
}

int main(void)
{
	uint16_t distance;

	leds.begin();
	setAllLedsToColor(Color::Black);
	leds.show();
	
	PORTB |= (1 << PINB3); // Enable pull-up resistor on PB3 (switch)
	
	TCCR0B |= (1 << CS00);	// Clk / 1
	OCR0A = (F_CPU / 1000000UL) * MICROSECONDS_PER_TICK - 1;
	TCCR0B |= WGM01;	// CTC timer mode
	TCNT0 = 0;	// Clear counter
	TIMSK |= (1 << OCIE0A);	// Enable output compare match interrupt
	
	sei();
			
	// TODO: Allow user to set minimum distance value
	uint16_t stopDistanceCm = 15;
	
	while(1)
	{
		distance = readDistance();	// Distance is in ticks. When MICROSECONDS_PER_TICK = 32, each tick is about a centimeter.
		if (distance == 0)
		{
			// No reading / timeout
			setAllLedsToColor(Color::Black);
		}
		else if (distance < stopDistanceCm * 32 / MICROSECONDS_PER_TICK)
		{
			setAllLedsToColor(Color::Red);
		}
		else if (distance > cautionDistanceCm * 32 / MICROSECONDS_PER_TICK)
		{
			setAllLedsToColor(Color::Lime);
		}
		else
		{
			uint16_t maxd = cautionDistanceCm * 32 / MICROSECONDS_PER_TICK;
			uint16_t mind = stopDistanceCm * 32 / MICROSECONDS_PER_TICK;
			uint16_t interval = (maxd - mind) / leds.numPixels();
			
			mind += interval;
			for (int i = leds.numPixels()-1; i >= 0; --i)
			{
				if (distance < mind)
				{
					leds.setPixelColor(i, Color::Yellow);
				}
				else
				{
					leds.setPixelColor(i, Color::Black);
				}
				mind += interval;
			}
			// setAllLedsToColor(Color::Yellow);
		}
		leds.show();
		
		// Allow sensor to recover
		_delay_ms(20);
	}
}

// This interrupt handler is called on TIMER0 compare. TIMER0 is configured to run at approximately 2us intervals.
// The interrupt handler does nothing but increment a 16-bit counter when PB1 is high. The logic to reset the counter,
// send the start pulse, etc. is handled in the main loop. This allows us to keep the interrupt handler very short,
// which in turn allows us to complete the ISR within the 2uS interval (about 16 clock cycles, minus overhead).
ISR(TIMER0_COMPA_vect)
{
	if (PINB & (1 << PINB1))
	{
		++echoTicks;
	}
}
