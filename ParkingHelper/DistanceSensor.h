/* 
* DistanceSensor.h
*
* Created: 10/20/2013 4:47:11 PM
* Author: Matthew
*/


#ifndef __DISTANCESENSOR_H__
#define __DISTANCESENSOR_H__

#include <avr/io.h>

// Abstraction over the Parallax Ping))) sensor. Uses 10uS timer interrupts for very accurate distance readings (< 2mm).
class DistanceSensor
{
//variables
public:
protected:
private:
	enum States
	{
		IDLE = 0,
		CAPTURING,
		RECOVERING			
	};
	
	uint16_t m_capture;
	bool m_hasCapture;
	uint8_t m_state;
	uint8_t m_ticksSinceStateChange;
	
//functions
public:
	DistanceSensor(uint8_t pin);
	~DistanceSensor();
	
	void startCapture();
	bool isReadyForCapture();
	
	bool hasCapture();
	float getCapture();
	float getCaptureAndClear();
	
	// Call this method with the slow timer interrupt, to capture distance readings and update internal state
	void tick();
	
protected:
private:
	void disableInterrupt();
	void enableInterrupt();
	float captureTimeToCm(float echoTicks);	
	bool checkForCompleteCapture();
	void processCapture(uint16_t capture);
	void checkForCompleteRecovery();
	
	DistanceSensor( const DistanceSensor &c );
	DistanceSensor& operator=( const DistanceSensor &c );

}; //DistanceSensor

#endif //__DISTANCESENSOR_H__
