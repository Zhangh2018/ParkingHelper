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
#include <avr/eeprom.h>
#include "LPD8806tiny.h"
#include "LedSequencer.h"
#include "DistanceSensor.h"

const uint8_t MSECS_PER_SLOW_INT = 1;
const uint8_t SEQUENCER_TICK_DIVISOR = 10;
const float DEFAULT_STOP_DISTANCE = 15.0f;
const float DANGER_CLOSE_DELTA = 3.0f;
const float CAUTION_DISTANCE = 150.0f;
const uint32_t MOTIONLESS_TICKS_TO_IDLE = 120000;
const float MOTION_THRESHOLD_CM = 2;
const uint16_t IDLE_CAPTURE_INTERVAL = 10000;
const uint16_t PROGRAM_COUNTDOWN_SEGMENT_TICKS = 10000;
const uint16_t PROGRAM_COUNTDOWN_SEGMENTS = 5;
const uint32_t EE_SIGNATURE = 0x4d4b4d43;

uint32_t EEMEM ee_signature;
float EEMEM ee_stopDistance;

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
	
uint8_t allBlueRedOne[] = { Color_Red, Color_Blue, Color_Blue, Color_Blue};
uint8_t allBlueRedTwo[] = { Color_Blue, Color_Red, Color_Blue, Color_Blue};
uint8_t allBlueRedThree[] = { Color_Blue, Color_Blue, Color_Red, Color_Blue};
uint8_t allBlueRedFour[] = { Color_Blue, Color_Blue, Color_Blue, Color_Red};

Segment seqAllBlack[] = { { allBlack, 255} };
Segment seqCautionOne[] =  { { oneYellow, 50},  { allBlack, 50} };
Segment seqCautionTwo[] =  { { twoYellow, 50},  { oneYellow, 50} };
Segment seqCautionThree[] =  { { threeYellow, 50},  { twoYellow, 50} };
Segment seqCautionFour[] =  { { allYellow, 50},  { threeYellow, 50} };
Segment seqWelcomeAboard[] = { {oneGreen, 25}, {twoGreen, 25}, {threeGreen, 25}, {allGreen, 50} };
Segment seqStop[] = { {allRed, 255} };
Segment seqDangerClose[] = { {allRed, 10}, {allBlack, 10} };
	
Segment seqProgramCountdown[] = { {allBlueRedOne, 10}, {allBlueRedTwo, 10}, {allBlueRedThree, 10}, {allBlueRedFour, 10}, {allBlueRedFour, 10}, {allBlueRedThree, 10}, {allBlueRedTwo, 10}, {allBlueRedOne, 10} };
Segment seqConfirmProgram[] = { { allBlue, 20}, { allRed , 10} };

class ParkingHelper
{
public:
	ParkingHelper(uint8_t numLeds);
	~ParkingHelper();
	
	void tick();
private:
	void setAllLedsToColor(const Color& color);
	void setPatternForDistance(float distance);
	void doIdle();
	void doActive();
	void doProgram();
	void goIdle();
	void goActive();
	void goProgram();
	bool isButtonPressed();
	void loadStopDistance();
	void saveStopDistance();
	
	enum State
	{
		IDLE,
		ACTIVE,
		PROGRAM
	};
	
	uint8_t m_state;
	LPD8806 m_leds;
	LedSequencer m_sequencer;
	DistanceSensor m_distanceSensor;
	uint32_t m_millis;
	uint32_t m_ticksSinceMotion;
	float m_lastDistance;
	uint16_t m_idleCaptureTicks;
	uint16_t m_programCountdown;
	uint16_t m_programSegment;
	float m_stopDistance;
};

ParkingHelper g_parkingHelper(4);

ParkingHelper::ParkingHelper(uint8_t numLeds)
	: m_state(ACTIVE), 
	m_leds(numLeds, PB0 /* data */, PB2 /* clock */),
	m_sequencer(&m_leds, colorTable, NELEMS(colorTable), SEQUENCER_TICK_DIVISOR),
	m_distanceSensor(PB1),
	m_millis(0),
	m_ticksSinceMotion(0),
	m_lastDistance(0.0f),
	m_idleCaptureTicks(0),
	m_programCountdown(0),
	m_programSegment(0),
	m_stopDistance(DEFAULT_STOP_DISTANCE)
{
	m_leds.begin();	
	setAllLedsToColor(Color::Black);

	PORTB |= _BV(PB3) | _BV(PB4); // Enable pull-up resistor on inputs PB3 (switch) and PB4 (unused pin)	

	loadStopDistance();	// Load stop distance from EEPROM	
}

ParkingHelper::~ParkingHelper()
{
	
}

void ParkingHelper::loadStopDistance()
{
	uint32_t signature = eeprom_read_dword(&ee_signature);
	if (signature == EE_SIGNATURE)
	{
		m_stopDistance = eeprom_read_float(&ee_stopDistance);			
	}
	else
	{
		m_stopDistance = DEFAULT_STOP_DISTANCE;
	}
}

void ParkingHelper::saveStopDistance()
{
	eeprom_write_float(&ee_stopDistance, m_stopDistance);
	eeprom_write_dword(&ee_signature, EE_SIGNATURE);
}

bool ParkingHelper::isButtonPressed()
{
	return !(PINB & _BV(PB3));
}

void ParkingHelper::tick()
{
	++m_millis;

	m_distanceSensor.tick();
	
	// TODO: Replace c-style switch statement with implementation of State pattern
	switch(m_state)
	{
	case IDLE:
		doIdle();
		break;
		
	case ACTIVE:
		doActive();
		break;
		
	case PROGRAM:
		doProgram();
		break;
	}
	
	m_sequencer.tick();
}

void ParkingHelper::goIdle()
{
	// Clear display and leave it cleared
	m_sequencer.clear();
	m_sequencer.setTickDivisor(SEQUENCER_TICK_DIVISOR);
	m_ticksSinceMotion = 0;
	m_idleCaptureTicks = 0;
	m_state = IDLE;
}

void ParkingHelper::goActive()
{
	m_sequencer.clear();
	m_sequencer.setTickDivisor(SEQUENCER_TICK_DIVISOR);
	m_ticksSinceMotion = 0;
	m_idleCaptureTicks = 0;
	m_state = ACTIVE;
}

void ParkingHelper::goProgram()
{
	m_programCountdown = PROGRAM_COUNTDOWN_SEGMENT_TICKS;
	m_programSegment = PROGRAM_COUNTDOWN_SEGMENTS;
	m_sequencer.startSequence(seqProgramCountdown, NELEMS(seqProgramCountdown), true);
	m_sequencer.setTickDivisor(10 * m_programSegment);
	m_state = PROGRAM;
}

void ParkingHelper::doProgram()
{
	m_distanceSensor.startCapture();	// Code will ignore request if not ready

	if (--m_programCountdown == 0)
	{
		if (--m_programSegment == 0)
		{
			// Program the setting
			m_sequencer.setTickDivisor(SEQUENCER_TICK_DIVISOR);
			for (int i = 0; i < 5; i++)
			{
				m_sequencer.playSequence(seqConfirmProgram, NELEMS(seqConfirmProgram), 1);
				m_stopDistance = m_distanceSensor.getCaptureAndClear();
				saveStopDistance();
			}
			goActive();
			return;
		}
		m_programCountdown = PROGRAM_COUNTDOWN_SEGMENT_TICKS;
		m_sequencer.setTickDivisor(10 * m_programSegment);
	}
}

void ParkingHelper::doIdle()
{
	// In idle mode we capture distance readings every few seconds and do not display anything
	// If we detect motion, we return to active mode
	
	if (isButtonPressed())
	{
		goProgram();
	}
	
	if (m_distanceSensor.hasCapture())
	{
		float distance = m_distanceSensor.getCaptureAndClear();
		float delta = m_lastDistance > distance ? m_lastDistance - distance : distance - m_lastDistance;
		m_lastDistance = distance;
		if (delta > MOTION_THRESHOLD_CM)
		{
			goActive();
			return;
		}
	}
	
	if (m_idleCaptureTicks >= IDLE_CAPTURE_INTERVAL)
	{
		if (m_distanceSensor.isReadyForCapture())
		{			
			m_idleCaptureTicks = 0;
			m_distanceSensor.startCapture();
		}
	}
	else
	{
		++m_idleCaptureTicks;
	}
}

void ParkingHelper::doActive()
{
	// In active mode we capture and display distance readings as fast as the sensor can go
	
	if (isButtonPressed())
	{
		goProgram();
	}
	
	++m_ticksSinceMotion;
	
	if (m_distanceSensor.hasCapture())
	{
		float distance = m_distanceSensor.getCaptureAndClear();	
		float delta = m_lastDistance > distance ? m_lastDistance - distance : distance - m_lastDistance;
		m_lastDistance = distance;
		if (delta > MOTION_THRESHOLD_CM)
		{
			m_ticksSinceMotion = 0;
		}
		else if (m_ticksSinceMotion >= MOTIONLESS_TICKS_TO_IDLE)
		{
			goIdle();
			return;
		}
		
		setPatternForDistance(distance);
	}

	m_distanceSensor.startCapture();	// Code will ignore request if not ready
}

void ParkingHelper::setAllLedsToColor(const Color& color)
{
	for (uint16_t i = 0; i < m_leds.numPixels(); ++i)
	{
		m_leds.setPixelColor(i, color);
	}
	m_leds.show();
}

void ParkingHelper::setPatternForDistance(float distance)
{
	if (distance == 0)
	{
		// No reading / timeout
		m_sequencer.startSequenceIfDifferent(seqAllBlack, NELEMS(seqAllBlack), false);
	}
	else if (distance < m_stopDistance - DANGER_CLOSE_DELTA)
	{
		m_sequencer.startSequenceIfDifferent(seqDangerClose, NELEMS(seqDangerClose), true);
	}
	else if (distance < m_stopDistance)
	{
		m_sequencer.startSequenceIfDifferent(seqStop, NELEMS(seqStop), false);	
	}
	else if (distance > CAUTION_DISTANCE)
	{
		m_sequencer.startSequenceIfDifferent(seqWelcomeAboard, NELEMS(seqWelcomeAboard), true);
	}
	else
	{
		float quarter = (CAUTION_DISTANCE - m_stopDistance) / 4.0f;
		float d1 = CAUTION_DISTANCE - quarter;
		float d2 = d1 - quarter;
		float d3 = d2 - quarter;

		if (distance > d1)
		{
			m_sequencer.startSequenceIfDifferent(seqCautionOne, NELEMS(seqCautionOne), true);
		}
		else if (distance > d2)
		{
			m_sequencer.startSequenceIfDifferent(seqCautionTwo, NELEMS(seqCautionTwo), true);
		}
		else if (distance > d3)
		{
			m_sequencer.startSequenceIfDifferent(seqCautionThree, NELEMS(seqCautionThree), true);
		}
		else
		{
			m_sequencer.startSequenceIfDifferent(seqCautionFour, NELEMS(seqCautionFour), true);
		}
	}
}

int main(void)
{
	// Configure Timer1 for 1 ms interrupts
	TCCR1 |= _BV(CTC1) | _BV(CS13);	// CTC mode, pre-scaler -> CPU clock / 128
	OCR1C = (F_CPU / 128UL / 1000UL) * MSECS_PER_SLOW_INT - 1; // Clear counter at 125
	OCR1A = OCR1C;	// On the ATTiny, the OCR1A or OCR1B must be used to generate the actual interrupt
	TIMSK |= _BV(OCIE1A); // Enable output match compare interrupt	
	
	// Enable interrupts
	sei();

	
	while(true)
	{
		set_sleep_mode(SLEEP_MODE_IDLE);
		cli();
		sleep_enable();
		sei();
		sleep_cpu();			
		// CPU is asleep here			
		sleep_disable();
	}
	
}

// The ISR_NOBLOCK allows nested interrupts so that the hi-res timer (Timer0) handler will not be blocked by this low-res timer code. 
// This improves accuracy of the distance measurements. We specifically disable the Timer1 interrupt during this routine so that we
// do not have nested Timer1 interrupts (and the potential for stack overflow).
ISR(TIMER1_COMPA_vect, ISR_NOBLOCK)
{
	// Disable this interrupt because we do not want to allow THIS interrupt to nest, potentially leading to a stack overflow situation
	TIMSK &= ~_BV(OCIE1A);
	
	g_parkingHelper.tick();
	
	// Safely re-enable this interrupt
	cli();
	TIMSK |= _BV(OCIE1A);
	// Global interrupts will be enabled upon execution of the RETI instruction
}
