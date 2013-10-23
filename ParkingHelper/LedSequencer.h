/* 
* LedSequencer.h
*
* Created: 10/19/2013 5:58:19 PM
* Author:  matthew-humphrey
*/


#ifndef __LEDSEQUENCER_H__
#define __LEDSEQUENCER_H__

#include "LPD8806tiny.h"
#include <avr/io.h>

#define NELEMS(A) (sizeof(A) / sizeof A[0])

struct Segment
{
	uint8_t* pattern;
	uint8_t duration;
};

//struct Sequence
//{
	//Segment* segments;
	//uint8_t numSegments;
//};

// Manages an LED strip so as to make the lights blink
class LedSequencer
{
private:
	LPD8806* m_leds;
	const Color* m_colorTable;
	uint8_t m_colorTableLength;
	Segment* m_segments;
	uint8_t m_numSegments;
	uint8_t m_currentSegmentIndex;
	uint8_t m_currentSegmentProgress;
	bool m_autoRepeat;
	uint8_t m_tickDivisor;
	uint8_t m_subTickCount;
	
public:
	LedSequencer(LPD8806* leds, const Color* colorTable, uint8_t colorTableLength, uint8_t tickDivisor);
	~LedSequencer();
	void startSequenceIfDifferent(Segment* segments, uint8_t numSegments, bool autoRepeat);
	void startSequence(Segment* segments, uint8_t numSegments, bool autoRepeat);
	void playSequence(Segment* segments, uint8_t numSegments, uint8_t millisecondsPerTick);
	bool tick();
	bool isSequenceActive();
	void clear();
	void setTickDivisor(uint8_t tickDivisor);
protected:
private:
	LedSequencer( const LedSequencer &c );
	LedSequencer& operator=( const LedSequencer &c );
	void showSegment(const Segment& segment);
	bool nextSegment();
	
}; //LedSequencer

#endif //__LEDSEQUENCER_H__
