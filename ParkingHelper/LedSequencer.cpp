/* 
* LedSequencer.cpp
*
* Created: 10/19/2013 5:58:18 PM
* Author: Matthew
*/

#include "LedSequencer.h"
#include <stddef.h>
#include <util/delay.h>

// default constructor
LedSequencer::LedSequencer(LPD8806* leds, const Color* colorTable, uint8_t colorTableLength, uint8_t tickDivisor)
	:m_leds(leds), m_colorTable(colorTable), m_colorTableLength(colorTableLength), m_segments(0), m_autoRepeat(false), m_tickDivisor(tickDivisor), m_subTickCount(0)
{
} //LedSequencer

// default destructor
LedSequencer::~LedSequencer()
{
} //~LedSequencer

void LedSequencer::startSequenceIfDifferent(Segment* segments, uint8_t numSegments, bool autoRepeat)
{
	if (m_segments != segments)
	{
		startSequence(segments, numSegments, autoRepeat);
	}
}

void LedSequencer::startSequence(Segment* segments, uint8_t numSegments, bool autoRepeat)
{
	m_segments = segments;
	m_numSegments = numSegments;
	m_autoRepeat = autoRepeat;
	m_currentSegmentIndex = 0;
	m_currentSegmentProgress = 0;
	m_subTickCount = 0;
	
	showSegment(m_segments[m_currentSegmentIndex]);
}

void LedSequencer::clear()
{
	m_numSegments = 0;
	m_segments = 0;
	for (uint16_t i = 0; i < m_leds->numPixels(); ++i)
	{
		m_leds->setPixelColor(i, Color::Black);
	}
	m_leds->show();
}

void LedSequencer::playSequence(Segment* segments, uint8_t numSegments, uint8_t millisecondsPerTick)
{
	uint8_t millis;
	startSequence(segments, numSegments, false);
	do 
	{
		millis = millisecondsPerTick;
		while (millis != 0)
		{
			_delay_ms(1);
			--millis;
		}
	} while (!tick());
}

void LedSequencer::setTickDivisor(uint8_t tickDivisor)
{
	m_tickDivisor = tickDivisor;
}

bool LedSequencer::tick()
{	
	if (!isSequenceActive())
	{
		// If we already finished, then don't do anything other than to keep returning true
		return true;
	}
	
	if (++m_subTickCount >= m_tickDivisor)
	{
		m_subTickCount = 0;

		uint8_t duration = m_segments[m_currentSegmentIndex].duration;
		++m_currentSegmentProgress;
		if (m_currentSegmentProgress >= duration)
		{
			return nextSegment();
		}
	}
	
	return false;
}

bool LedSequencer::nextSegment()
{
	m_currentSegmentProgress = 0;
	++m_currentSegmentIndex;
	if (m_currentSegmentIndex >= m_numSegments)
	{
		if (!m_autoRepeat)
		{
			return true;
		}
		m_currentSegmentIndex = 0;
	}
	
	showSegment(m_segments[m_currentSegmentIndex]);
	
	return m_currentSegmentIndex == 0;
}

void LedSequencer::showSegment(const Segment& segment)
{
	for (uint16_t i = 0; i < m_leds->numPixels(); ++i)
	{
		m_leds->setPixelColor(i, m_colorTable[segment.pattern[i]]);
	}
	m_leds->show();
}

bool LedSequencer::isSequenceActive()
{
	return !(m_segments == NULL || m_currentSegmentIndex >= m_numSegments);
}