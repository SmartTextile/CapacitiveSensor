/**
 * Capacitive Sensing Library for Arduino Due
 * Copyright (c) 2008 Paul Bagder  All rights reserved.
 * Version 04 by Paul Stoffregen - Arduino 1.0 compatibility, issue 146 fix
 * Version "Due-compatible" by Marco Lipparini
 * 
 * This file is part of the MLX Hardware project.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * version 2 along with this code.
 * If not, see <http://www.gnu.org/licenses/gpl-2.0.html>.
 *
 * Project website: http://www.mlx-service.it
 */

// Required libraries...
#include "CapacitiveSensorDue.h"
#include "Arduino.h"
#include <limits.h>

/*
 * Static fields...
 */
// Value determined empirically by the original author (Paul Stoffregen).
const uint8_t CapacitiveSensorDue::LOOP_TIMING_FACTOR = 310;

// Using floats to deal with large numbers.
#define TIMEOUT_FORMULA(TIMEOUT) (TIMEOUT * (float) CapacitiveSensorDue::LOOP_TIMING_FACTOR * (float) F_CPU) / 84000000

// Default timeout.
unsigned long CapacitiveSensorDue::_timeout = TIMEOUT_FORMULA(2000);

/*
 * Methods...
 */
// [Constructor]
CapacitiveSensorDue::CapacitiveSensorDue(uint8_t sendPin, uint8_t receivePin)
{
	Pio *sendPort;
	Pio *receivePort;
	
	this->_sendPin = sendPin;
	this->_receivePin = receivePin;
	this->_leastCheckedTime = ULONG_MAX;
	
	// Initializing send pin...
	this->_sendBitmask = digitalPinToBitMask(sendPin);
	sendPort = digitalPinToPort(sendPin);
	this->_sendOutRegister = portOutputRegister(sendPort);
	
	// Initializing receive pin...
	this->_receiveBitmask = digitalPinToBitMask(receivePin);
	receivePort = digitalPinToPort(receivePin);
	this->_receiveInRegister = portInputRegister(receivePort);
	this->_receiveOutRegister = portOutputRegister(receivePort);
	
	noInterrupts();
	pinMode(sendPin, OUTPUT);
	interrupts();
}

// [Set Timeout]
void CapacitiveSensorDue::setTimeout(unsigned long timeout) {
	CapacitiveSensorDue::_timeout = TIMEOUT_FORMULA(timeout);
}

// [Check Capacitance]
long CapacitiveSensorDue::check(uint8_t samples)
{
	long total = 0;
	
	if (samples == 0) {
		return 0;
	}
	
	for (uint8_t i = 0; i < samples; i++) {
		if (senseSample(&total) < 0) {
			// Timeout...
			return -1;
		}
	}
	
	if (total < this->_leastCheckedTime) {
		this->_leastCheckedTime = total;
	}
	
	return total - this->_leastCheckedTime;
}

// [Sense Sample]
int CapacitiveSensorDue::senseSample(long *total)
{
	noInterrupts();
	
	// Set send pin register LOW...
	*this->_sendOutRegister &= ~this->_sendBitmask;
	pinMode(this->_receivePin, INPUT);
	// Set receive pin register LOW to make sure pullups are off...
	*this->_receiveOutRegister &= ~this->_receiveBitmask;
	pinMode(this->_receivePin, OUTPUT); // Receive pin is now LOW AND OUTPUT...
	pinMode(this->_receivePin, INPUT);
	// Set send pin HIGH
	*this->_sendOutRegister |= this->_sendBitmask;
	
	interrupts();
	
	#define LOOP_CONDITION (*this->_receiveInRegister & this->_receiveBitmask) && (*total < CapacitiveSensorDue::_timeout)
	
	// While receive pin is LOW and timeout has not occurred...
	while (!LOOP_CONDITION) {
		(*total)++;
	}
	
	#define CHECK_TIMEOUT() \
		if (*total >= CapacitiveSensorDue::_timeout) { \
			return -1; \
		}
	CHECK_TIMEOUT();
	
	// Set receive pin HIGH briefly to charge up fully because the while loop above will
	// exit when pin is ~2.5V. 
	noInterrupts();
	
	// Set receive pin HIGH (turns on pullup)...
	*this->_receiveOutRegister |= this->_receiveBitmask;
	pinMode(this->_receivePin, OUTPUT);
	pinMode(this->_receivePin, INPUT);
	// Turn off pullup...
	*this->_receiveOutRegister &= ~this->_receiveBitmask;
	// Set send pin LOW
	*this->_sendOutRegister &= ~this->_sendBitmask;
	
	interrupts();
	
	// While receive pin is HIGH and timeout has not occurred...
	while (LOOP_CONDITION) {
		(*total)++;
	}
	
	CHECK_TIMEOUT();
	
	return 1;
}
