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
const uint8_t USECS_PER_FAST_INT = 10;
const uint8_t MSECS_PER_SLOW_INT = 1;

volatile uint32_t ticks = 0;
volatile uint16_t echoTicks = 0;

LPD8806 leds(NUM_LEDS, PB0 /* data */, PB2 /* clock */);

void setAllLedsToColor(const Color& color)
{
	for (uint16_t i = 0; i < leds.numPixels(); ++i)
	{
		leds.setPixelColor(i, color);
	}
}

float readDistanceCm()
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
		
	uint16_t duration;
	ATOMIC_BLOCK(ATOMIC_FORCEON)
	{
		duration = echoTicks;		
	}
	
	return (float)duration * (float)USECS_PER_FAST_INT * (34029.0f / 2.0f / 1000000.0f);
}

int main(void)
{
	leds.begin();
	setAllLedsToColor(Color::Black);
	leds.show();
	
	PORTB |= (1 << PINB3); // Enable pull-up resistor on PB3 (switch)
	
	// Configure Timer0 for 10 us interrupts
	TCCR0A |= _BV(WGM01);	// CTC timer mode
	TCCR0B |= _BV(CS00);	// Pre-scaler -> CPU clock / 1
	OCR0A = (F_CPU / 1000000UL) * USECS_PER_FAST_INT - 1;
	TCNT0 = 0;	// Clear counter
	TIMSK |= _BV(OCIE0A);	// Enable output compare match interrupt
	
	// Configure Timer1 for 1 ms interrupts
	TCCR1 |= _BV(CTC1) | _BV(CS13);	// CTC mode, pre-scaler -> CPU clock / 128
	OCR1C = (F_CPU / 128UL / 1000UL) * MSECS_PER_SLOW_INT - 1; // Clear counter at 125
	OCR1A = OCR1C;	// On the ATTiny, the OCR1A or OCR1B must be used to generate the actual interrupt
	TIMSK |= _BV(OCIE1A); // Enable output match compare interrupt
	
	sei();	// Enable global interrupts
			
	float distance;
	const float stopDistance = 15.0f;
	const float cautionDistance = 150.0f;
	
	while(1)
	{
		distance = readDistanceCm();
		if (distance == 0)
		{
			// No reading / timeout
			setAllLedsToColor(Color::Black);
		}
		else if (distance < stopDistance)
		{
			setAllLedsToColor(Color::Red);
		}
		else if (distance > cautionDistance)
		{
			setAllLedsToColor(Color::Lime);
		}
		else
		{
			float maxd = cautionDistance;
			float mind = stopDistance;
			float interval = (maxd - mind) / leds.numPixels();
			
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

// The ISR_NOBLOCK allows nested interrupts so that the hi-res timer (Timer0) handler will not be blocked by this low-res timer code. 
// This improves accuracy of the distance measurements. We specifically disable the Timer1 interrupt during this routine so that we
// do not have nested Timer1 interrupts (and the potential for stack overflow).
ISR(TIMER1_COMPA_vect, ISR_NOBLOCK)
{
	// Disable this interrupt because we do not want to allow THIS interrupt to nest, potentially leading to a stack overflow situation
	TIMSK &= ~_BV(OCIE1A);
	
	++ticks;	

	volatile static uint8_t tenTicks = 0;
	if (++tenTicks >= 10)
	{
		// TODO: Call LED sequencer
		tenTicks = 0;
	}
		
	// Safely re-enable this interrupt
	cli();
	TIMSK |= _BV(OCIE1A);
	// Global interrupts will be enabled upon execution of the RETI instruction
}