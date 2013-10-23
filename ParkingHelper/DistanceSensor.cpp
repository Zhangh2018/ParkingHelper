/* 
* DistanceSensor.cpp
*
* Created: 10/20/2013 4:47:11 PM
* Author: Matthew
*/

#include "DistanceSensor.h"
#include <stdlib.h>
#include <string.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

const uint8_t USECS_PER_TICK = 10;
const uint32_t TIMEOUT_CYCLES = F_CPU / 1000 * 50 / 4;
const uint8_t RECOVERY_TICKS = 80;
const uint8_t TIMEOUT_TICKS = 50;

volatile static uint16_t g_echoTicks = 0;
static uint8_t g_pinMask;

// default constructor
DistanceSensor::DistanceSensor(uint8_t pin)
	: m_capture(0), m_hasCapture(false), m_state(IDLE), m_ticksSinceStateChange(0)
{	
	g_pinMask = _BV(pin);

	g_echoTicks = 0;
	
	// Configure Timer0 for 10 us interrupts
	TCCR0A |= _BV(WGM01);	// CTC timer mode
	TCCR0B |= _BV(CS00);	// Pre-scaler -> CPU clock / 1
	OCR0A = (F_CPU / 1000000UL) * USECS_PER_TICK - 1;	// 10us interrupts
} //DistanceSensor

// default destructor
DistanceSensor::~DistanceSensor()
{
} //~DistanceSensor

float DistanceSensor::getCapture()
{
	return captureTimeToCm(m_capture);
}

float DistanceSensor::getCaptureAndClear()
{
	m_hasCapture = false;
	return captureTimeToCm(m_capture);
}

bool DistanceSensor::hasCapture()
{
	return m_hasCapture;
}

bool DistanceSensor::isReadyForCapture()
{
	return m_state == IDLE;
}

float DistanceSensor::captureTimeToCm(float echoTicks)
{
	return (float)echoTicks * (float)USECS_PER_TICK * (34029.0f / 2.0f / 1000000.0f);
}

void DistanceSensor::startCapture()
{
	if (m_state != IDLE)
	{
		return;
	}

	m_state = CAPTURING;
	m_hasCapture = false;
	m_ticksSinceStateChange = 0;
	
	// Trigger distance reading
	DDRB |= (1 << DDB1);	// Set as output
	PORTB |= (1 << PB1);	// Set HIGH
	_delay_us(3);
	PORTB &= ~(1 << PB1);	// Set LOW
	DDRB &= ~(1 << DDB1);	// Set as input
	_delay_us(50);			// Allow trigger line to stabilize
	
	g_echoTicks = 0;
	enableInterrupt();
}

void DistanceSensor::enableInterrupt()
{
	TCNT0 = 0;				// Reset counter
	TIFR |= _BV(OCF0A);		// Enable interrupt 
	TIMSK |= _BV(OCIE0A);	// Enable output compare match interrupt	
}

void DistanceSensor::disableInterrupt()
{
	TIMSK &= ~(_BV(OCIE0A)); // Disable output compare match interrupt
}

void DistanceSensor::tick()
{
	// TODO: replace ugly C-style switch statement with State pattern
	switch (m_state)
	{
	case CAPTURING:
		checkForCompleteCapture();
		
	case RECOVERING:
		checkForCompleteRecovery();	
		break;
		
	case IDLE:
		break;
	}
}

bool DistanceSensor::checkForCompleteCapture()
{
	++m_ticksSinceStateChange;
	
	if (PINB & g_pinMask)
	{
		// Pin is still high, which means a capture is still underway
		return false;
	}
	
	// Pin value is low. This either means a capture completed or the sensor hasn't started the reading yet 
	// (or something is wrong like the sensor isn't connected)
	uint16_t duration;
	ATOMIC_BLOCK(ATOMIC_FORCEON)
	{
		duration = g_echoTicks;
	}
	
	if (!duration && m_ticksSinceStateChange < TIMEOUT_TICKS)
	{
		// No reading yet
		return false;
	}
	
	// We have a capture
	m_capture = duration;
	m_hasCapture = true;
	
	g_echoTicks = 0;
	m_ticksSinceStateChange = 0;
	m_state = RECOVERING;
	
	return true;
}

void DistanceSensor::checkForCompleteRecovery()
{
	++m_ticksSinceStateChange;
	if ( m_ticksSinceStateChange > RECOVERY_TICKS)
	{
		m_ticksSinceStateChange = 0;
		m_state = IDLE;
	}
}

// This interrupt handler is called on TIMER0 compare. TIMER0 is configured to run at approximately 10us intervals.
// The interrupt handler does nothing but increment a 16-bit counter when PB1 is high. The logic to reset the counter,
// send the start pulse, etc. is handled in the main loop. This allows us to keep the interrupt handler very short,
// which in turn allows us to complete the ISR within the 2uS interval (about 16 clock cycles, minus overhead).
ISR(TIMER0_COMPA_vect)
{
	if (PINB & g_pinMask)
	{
		++g_echoTicks;
	}
}

