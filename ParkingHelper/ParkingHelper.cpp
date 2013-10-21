/*
 * ParkingHelper.cpp
 *
 * Created: 10/14/2013 2:29:40 PM
 *  Author: Matthew
 */ 

#include <stddef.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "LPD8806tiny.h"
#include "LedSequencer.h"
#include "DistanceSensor.h"

const uint8_t NUM_LEDS = 4;
const uint32_t TIMEOUT_CYCLES = F_CPU / 1000 * 20;
const uint8_t USECS_PER_FAST_INT = 10;
const uint8_t MSECS_PER_SLOW_INT = 1;

const float STOP_DISTANCE = 15.0f;
const float DANGER_CLOSE_DELTA = 3.0f;
const float CAUTION_DISTANCE = 150.0f;

volatile uint32_t ticks = 0;

Color colorTable[] = { Color::Black, Color::Red, Color::Green, Color::Blue, Color::Yellow };
enum ColorOffsets { Color_Black = 0, Color_Red, Color_Green, Color_Blue, Color_Yellow };

//  Color patterns
uint8_t allBlack[] = { Color_Black, Color_Black, Color_Black, Color_Black};
uint8_t allYellow[] = { Color_Yellow, Color_Yellow, Color_Yellow, Color_Yellow };
uint8_t allRed[] = { Color_Red, Color_Red, Color_Red, Color_Red};
uint8_t oneGreen[] = { Color_Green, Color_Black, Color_Black, Color_Black};
uint8_t twoGreen[] = { Color_Green, Color_Green, Color_Black, Color_Black};
uint8_t threeGreen[] = { Color_Green, Color_Green, Color_Green, Color_Black};
uint8_t allGreen[] = { Color_Green, Color_Green, Color_Green, Color_Green};
uint8_t allBlue[] = { Color_Blue, Color_Blue, Color_Blue, Color_Blue};
uint8_t oneYellow[] = { Color_Yellow, Color_Black, Color_Black, Color_Black};
uint8_t twoYellow[] = { Color_Yellow, Color_Yellow, Color_Black, Color_Black};
uint8_t threeYellow[] = { Color_Yellow, Color_Yellow, Color_Yellow, Color_Black};

Segment seqAllBlack[] = { { allBlack, 255} };
Segment seqFlashRedBlue[] = { { allRed , 25},  { allBlue, 50} };
Segment seqBlinkOneYellow[] =  { { oneYellow, 50},  { allBlack, 50} };
Segment seqBlinkTwoYellow[] =  { { twoYellow, 50},  { oneYellow, 50} };
Segment seqBlinkThreeYellow[] =  { { threeYellow, 50},  { twoYellow, 50} };
Segment seqBlinkFourYellow[] =  { { allYellow, 50},  { threeYellow, 50} };
Segment seqGreenRunway[] = { {oneGreen, 25}, {twoGreen, 25}, {threeGreen, 25}, {allGreen, 50} };
Segment seqSolidRed[] = { {allRed, 255} };
Segment seqDangerClose[] = { {allRed, 10}, {allBlack, 10} };

LPD8806 leds(NUM_LEDS, PB0 /* data */, PB2 /* clock */);
LedSequencer sequencer(&leds, colorTable, NELEMS(colorTable), 10);

DistanceSensor g_distanceSensor(PB1, 5);

void setAllLedsToColor(const Color& color)
{
	for (uint16_t i = 0; i < leds.numPixels(); ++i)
	{
		leds.setPixelColor(i, color);
	}
}

void setPatternForDistance(float distance)
{
	if (distance == 0)
	{
		// No reading / timeout
		sequencer.startSequenceIfDifferent(seqAllBlack, NELEMS(seqAllBlack), false);
	}
	else if (distance < STOP_DISTANCE - DANGER_CLOSE_DELTA)
	{
		sequencer.startSequenceIfDifferent(seqDangerClose, NELEMS(seqDangerClose), true);
	}
	else if (distance < STOP_DISTANCE)
	{
		sequencer.startSequenceIfDifferent(seqSolidRed, NELEMS(seqSolidRed), false);	
	}
	else if (distance > CAUTION_DISTANCE)
	{
		sequencer.startSequenceIfDifferent(seqGreenRunway, NELEMS(seqGreenRunway), true);
	}
	else
	{
		float quarter = (CAUTION_DISTANCE - STOP_DISTANCE) / 4.0f;
		float d1 = CAUTION_DISTANCE - quarter;
		float d2 = d1 - quarter;
		float d3 = d2 - quarter;

		if (distance > d1)
		{
			sequencer.startSequenceIfDifferent(seqBlinkOneYellow, NELEMS(seqBlinkOneYellow), true);
		}
		else if (distance > d2)
		{
			sequencer.startSequenceIfDifferent(seqBlinkTwoYellow, NELEMS(seqBlinkTwoYellow), true);
		}
		else if (distance > d3)
		{
			sequencer.startSequenceIfDifferent(seqBlinkThreeYellow, NELEMS(seqBlinkThreeYellow), true);
		}
		else
		{
			sequencer.startSequenceIfDifferent(seqBlinkFourYellow, NELEMS(seqBlinkFourYellow), true);
		}
	}
	
}

int main(void)
{
	leds.begin();
	setAllLedsToColor(Color::Black);
	leds.show();

	PORTB |= (1 << PINB3); // Enable pull-up resistor on PB3 (switch)
	
	g_distanceSensor.enable();
	
	// Configure Timer1 for 1 ms interrupts
	TCCR1 |= _BV(CTC1) | _BV(CS13);	// CTC mode, pre-scaler -> CPU clock / 128
	OCR1C = (F_CPU / 128UL / 1000UL) * MSECS_PER_SLOW_INT - 1; // Clear counter at 125
	OCR1A = OCR1C;	// On the ATTiny, the OCR1A or OCR1B must be used to generate the actual interrupt
	TIMSK |= _BV(OCIE1A); // Enable output match compare interrupt
	
	sei();
	
	while(true)
	{
		
		/*
		uint8_t saveTIMSK = TIMSK;
		
		cli();
		TIMSK = 0; // &= ~(_BV(OCIE0A) | _BV(OCIE1A));	// Disable timer interrupts		
		PCMSK |= _BV(PCINT3) | _BV(PCINT4);		// Unmask pin change interrupt for PB3 (pushbutton) and PB4 (Audio Detector)
		GIMSK |= _BV(PCIE);						// Globally enable pin-change interrupts	
		set_sleep_mode(SLEEP_MODE_PWR_DOWN);
		sleep_enable();
		
		sei();
		sleep_cpu();
		
		// CPU is asleep here
		
		sleep_disable();		
		
		TIMSK |= saveTIMSK;						// Re-enable timer interrupts
		PCMSK &= ~(_BV(PCINT3) | _BV(PCINT4));	// Mask pin change interrupt for PB3 (pushbutton) and PB4 (Audio Detector)
		GIMSK &= ~(_BV(PCIE));					// Globally disable pin-change interrupts
		*/
		
	}
	
	/*		
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
	*/
}

// The ISR_NOBLOCK allows nested interrupts so that the hi-res timer (Timer0) handler will not be blocked by this low-res timer code. 
// This improves accuracy of the distance measurements. We specifically disable the Timer1 interrupt during this routine so that we
// do not have nested Timer1 interrupts (and the potential for stack overflow).
ISR(TIMER1_COMPA_vect, ISR_NOBLOCK)
{
	// Disable this interrupt because we do not want to allow THIS interrupt to nest, potentially leading to a stack overflow situation
	TIMSK &= ~_BV(OCIE1A);
	
	++ticks;

	if (g_distanceSensor.tick())
	{
		float distance = g_distanceSensor.getAvgDistanceCm();
		setPatternForDistance(distance);
	}
		
	sequencer.tick();
	
	// Safely re-enable this interrupt
	cli();
	TIMSK |= _BV(OCIE1A);
	// Global interrupts will be enabled upon execution of the RETI instruction
}

ISR(PCINT0_vect)
{
	// Do nothing. We just use this to wake up.	
}