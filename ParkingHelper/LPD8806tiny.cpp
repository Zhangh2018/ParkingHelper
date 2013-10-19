#include "LPD8806tiny.h"
#include <stdlib.h>
#include <string.h>

// Arduino library to control LPD8806-based RGB LED Strips
// (c) Adafruit industries
// MIT license

/*****************************************************************************/

// Constructor for use with arbitrary clock/data pins:
LPD8806::LPD8806(uint16_t n, uint8_t dpin, uint8_t cpin) {
	pixels = 0;
	begun  = false;
	updateLength(n);
	updatePins(dpin, cpin);
}


// Activate hard/soft SPI as appropriate:
void LPD8806::begin(void) {
	startBitbang();
	begun = true;
}

// Change pin assignments post-constructor, using arbitrary pins:
void LPD8806::updatePins(uint8_t dpin, uint8_t cpin) {

	datapin     = dpin;
	clkpin      = cpin;
	clkpinmask  = (1 << cpin);
	datapinmask = (1 << dpin);

	if(begun == true) { // If begin() was previously invoked...
		startBitbang(); // Regardless, now enable 'soft' SPI outputs
	} // Otherwise, pins are not set to outputs until begin() is called.

}
// Enable software SPI pins and issue initial latch:
void LPD8806::startBitbang() {
	DDRB |= datapinmask;
	DDRB |= clkpinmask;
	PORTB &= ~datapinmask; // Data is held low throughout (latch = 0)
	for(uint8_t i = 8; i>0; i--) {
		PORTB |=  clkpinmask;
		PORTB &= ~clkpinmask;
	}
}

// Change strip length (see notes with empty constructor, above):
void LPD8806::updateLength(uint16_t n) {
	if(pixels != 0) free(pixels); // Free existing data (if any)
	numLEDs = n;
	n      *= 3; // 3 bytes per pixel
	if(NULL != (pixels = (uint8_t *)malloc(n + 1))) { // Alloc new data
		memset(pixels, 0x80, n); // Init to RGB 'off' state
		pixels[n]    = 0;        // Last byte is always zero for latch
	} else numLEDs = 0;        // else malloc failed
	// 'begun' state does not change -- pins retain prior modes
}

uint16_t LPD8806::numPixels(void) {
	return numLEDs;
}

// This is how data is pushed to the strip.  Unfortunately, the company
// that makes the chip didnt release the protocol document or you need
// to sign an NDA or something stupid like that, but we reverse engineered
// this from a strip controller and it seems to work very nicely!
void LPD8806::show(void) {
	uint16_t i, n3 = numLEDs * 3 + 1; // 3 bytes per LED + 1 for latch
	uint8_t pixel;
	
	for (i=0; i<n3; i++ ) {
		pixel = pixels[i] >> 1;	// Down-sample 8-bit to 7-bit color
		for (uint8_t bit=0x80; bit; bit >>= 1) {
			if(pixel & bit) PORTB |=  datapinmask;
			else                PORTB &= ~datapinmask;
			PORTB |=  clkpinmask;
			PORTB &= ~clkpinmask;
		}
	}
}

// Set pixel color from separate 7-bit R, G, B components:
void LPD8806::setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
	if(n < numLEDs) { // Arrays are 0-indexed, thus NOT '<='
		uint8_t *p = &pixels[n * 3];
		*p++ = b | 0x80; // Our LPD8806 strip color order is BRG (AdaFruit code was GRB),
		*p++ = r | 0x80; // not the more common RGB,
		*p++ = g | 0x80; 
	}
}

void LPD8806::setPixelColor(uint16_t n, const Color& color)
{
	if(n < numLEDs) { // Arrays are 0-indexed, thus NOT '<='
		uint8_t *p = &pixels[n * 3];
		*p++ = color.b | 0x80; // Our LPD8806 strip color order is BRG (AdaFruit code was GRB),
		*p++ = color.r | 0x80; // not the more common RGB,
		*p++ = color.g | 0x80;
	}
}


