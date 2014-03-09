/****
 *
 *   Part of the "Bendulum" library for Arduino. Version 1.22
 *
 *   Bendulum.cpp Copyright 2013 by D. L. Ehnebuske 
 *   License terms: Creative Commons Attribution-ShareAlike 3.0 United States (CC BY-SA 3.0 US) 
 *                  See http://creativecommons.org/licenses/by-sa/3.0/us/ for specifics. 
 *
 *   A "bendulum" is a long, slender, springy piece of metal, held vertically, whose lower end is fixed and whose upper 
 *   end has a weight that is free to move back and forth -- a sort of upside-down pendulum. A bendulum terminates in a 
 *   magnet that moves across a many-turn coil of fine wire positioned at the center of its swing. The motion of the magnet 
 *   is detected as it passes over the coil by the current the magnet induces. Just after the magnet passes, a pulse of 
 *   current through the coil induces a magnetic field in it. The field gives the magnet a small push, keeping the bendulum 
 *   going. A Bendulum object works just as well for a pendulum whose bob has a magnet at its end.
 *
 *   The main method is beat(). This method is intended to be invoked repeatedly in the loop() function of an Arduino sketch.
 *   Each time the magnet passes, beat() returns the number of microseconds that have passed since the magnet passed the
 *   last time. In this way, the bendulum can be used to drive a time-of-day clock display.
 *
 *   A Bendulum object has three operational modes. The way the duration of a beat is determined depends on which mode the 
 *   Bendulum object is in:
 *
 *           Mode          Beat duration determined by
 *           ===========   ========================================================================================
 *           SETTLING      The duration returned is measured using the (corrected) Arduino real-time Clock.
 *           SCALING       The duration returned is measured using the (corrected) Arduino real-time Clock.
 *           CALIBRATING   A running average of beat duration is updated using the measured duration of the current 
 *                         beat. As in the SETTLING mode, the current duration is measured with the (corrected) 
 *                         Arduino real-time clock. The updated average is returned.
 *           CALFINISH     The duration returned is the value of the running average. No measurement is done.
 *           RUNNING       The duration returned is the value of the running average. No measurement is done.
 *
 *   Unless setRunMode() is used to change it, when beat() is first called the Bendulum object is in SETTLING mode. 
 *   It continues in this mode for getTgtSettle() cycles (one cycle = two beats). The purpose of this mode is to let
 *   the bendulum or pendulum settle into a regular motion since its motion is typically disturbed at startup from 
 *   having been given a start-up push by hand. Once it completes SETTLING the Bendulum object switches to SCALING mode 
 *   for getTgtScale() cycles. During SCALING mode, the peak voltage induced in the coil by the passing magnet is 
 *   determined and saved in peakScale. With SCALING over, the Bendulum object moves to CALIBRATING mode, in which it 
 *   remains for getTgtSmoothing() additional cycles. During CALIBRATING mode, the average duration of tick and tock 
 *   beats is measured and saved in tickAvg, tockAvg and their average -- uspb. At the end of CALIBRATING the Bendulum 
 *   object switches to CALFINISH mode for one beat. The CALFINISH mode serves as notice to the using sketch that 
 *   calibration is now complete. At that point the Bendulum object switches to RUNNING mode, in which it remains 
 *   indefinitely.
 *
 *   The net effect is that the Bendulum object automatically characterizes the bendulum or pendulum it is driving,
 *   first by measuring the strength of the pulses it induces in the coil and second by measuring the average duration 
 *   of its beat. Once the measurements are done, the Bendulum object assumes the bendulum or pendulum is isochronous.
 *
 *   This would work nearly perfectly except that the real-time clock in most Arduinos is stable but not too accurate 
 *   (it's a ceramic resonator, not a crystal). That is, real-time clock ticks are essentially equal to one another in 
 *   duration but their durations are not exactly the number of microseconds they should be. To correct for this, we use 
 *   a correction factor, getBias(). The value of getBias() is the number of tenths of a second per day by which the 
 *   real-time clock in the Arduino must be compensated in order for it to be accurate. This number may be set using 
 *   setBias() and incremented/decremented by using incrBias(). Positive bias means the real-time clock's "microseconds" 
 *   are shorter than real microseconds.
 *
 *   In addition to just instantiating a Bendulum object and letting it go through its modes, it's possible to change the
 *   mode as required using setRunMode(). For example, to recalibrate, simply do a setRunMode(CALIBRATING). The bendulum 
 *   will enter CALIBRATING mode, do a calibration run and then enter CALFINISH for a beat and, finally, settle into 
 *   RUNNING mode.
 *
 *   It is also possible to operate a bendulum whose parameters you *know* and and skip all the automatic calibration
 *   stuff. After instantiating the Bendulum object, put it in RUNNING mode by invoking setRunMode(RUNNING) and then set 
 *   the beat duration with setBeatDuration() and the peak scaling with setPeakScale().
 *
 ****/
 
#include "Bendulum.h"

// Class Bendulum

/*
 *
 * Constructors
 *
 */
// Bendulum on specified sense and kick pins
Bendulum::Bendulum(byte sPin, byte kPin){
	sensePin = sPin;						// Pin on which we sense the bendulum's passing
	kickPin = kPin;							// Pin on which we kick the bendulum as it passes

	analogReference(EXTERNAL);				// We have an external reference, a 47k+47k voltage divider between 
											//   3.3V and ground
	pinMode(sensePin, INPUT);				// Set sense pin to INPUT since we read from it
	pinMode(kickPin, INPUT);				// Put the kick pin in INPUT (high impedance) mode so that the
											//   induced current doesn't flow to ground

	cycleCounter = 1;						// Current cycle counter for SETTLING and SCALING modes
	tgtSettle = 32;							// Number of cycles to run in SETTLING mode
	tgtScale = 128;							// Number of cycles to run in SCALING mode
	tgtSmoothing = 2048;					// Target smoothing interval in cycles
	curSmoothing = 1;						// Current smoothing interval in cycles
	bias = 0;								// Arduino clock correction in tenths of a second per day
	peakScale = 10;							// Peak scaling value (adjusted during calibration)
	tick = true;							// Whether currently awaiting a tick or a tock
	tickAvg = 0;							// Average period of ticks (μs)
	tockAvg = 0;							// Average period of tocks (μs)
	tickPeriod = 0;							// Length of last tick period (μs)
	tockPeriod = 0;							// Length of last tock period (μs)
	timeBeforeLast = lastTime = 0;			// Clock time (μs) last time through beat() (and time before that)
	runMode = SETTLING;						// Run mode -- SETTLING, SCALING, CALIBRATING, CALFINISH or RUNNING
}

/*
 *
 * Operational methods
 *
 */

// Do one beat return length of a beat in μs
long Bendulum::beat(){
	const int settleTime = 250;					// Time (ms) to delay to let things settle before looking for voltage spike
	const int delayTime = 5;					// Time in ms by which to delay the start of the kick pulse
	const int kickTime = 50;					// Duration in ms of the kick pulse
	
	const int maxPeak = 1;						// Scale the peaks (using peakScale) so they're no bigger than this
	
	int currCoil = 0;							// The value read from coilPin. The value read here, in volts, is 1024/AREF,
												//   where AREF is the voltage on that pin. AREF is set by a 1:1 voltage
												//   divider between the 3.3V pin and Gnd, so 1.65V. Más o menos. The exact 
												//   value doesn't really matter since we're looking for a spike above noise.
	int pastCoil = 0;							// The previous value of currCoil
	unsigned long topTime = 0;					// Clock time (μs) at entry to loop()
	
	// watch for passing bendulum
	delay(settleTime);							// Wait for things to calm down
	do {										// Wait for the voltage to fall to zero
		currCoil = analogRead(sensePin);
	} while (currCoil > 0);
	while (currCoil >= pastCoil) {				// While the bendulum hasn't passed over coil,
		pastCoil = currCoil;					//   loop waiting for the voltage induced in the coil to begin to fall
		currCoil = analogRead(sensePin) / peakScale;
	}
	
	topTime= micros();							// Remember when bendulum went by
	
	// Kick the bendulum to keep it going
	pinMode(kickPin, OUTPUT);					// Prepare kick pin for output
	delay(delayTime);							// Wait desired time before pin turn-on
	digitalWrite(kickPin, HIGH);				// Turn kick pin on
	delay(kickTime);							// Wait for duration of pulse
	digitalWrite(kickPin, LOW);					// Turn it off
	pinMode(kickPin, INPUT);					// Put kick pin in high impedance mode

	// Determine the length of time between beats in μs
	if (lastTime == 0) {						// if first time through
		lastTime = topTime;						//   Remember when we last saw the bendulum
		return 0;								//   Return 0 -- no interval between beats yet!
	}
	switch (runMode) {
		case SETTLING:							// When settling
			uspb = topTime - lastTime;			//   Microseconds per beat is whatever we measured for this beat
			uspb += ((bias * uspb) + 432000) / 864000; // plus the (rounded) Arduino clock correction
			if (uspb > 5000000) {				//   If the measured beat is more than 5 seconds long
				uspb = 0;						//     it can't be real -- just ignore it
			}
			if (tick) {							//   If tick
				tickPeriod = uspb;				//     Remember tickPeriod
			} else {							//   Else (tock)
				tockPeriod = uspb;				//     Remember tockPeriod
				if (++cycleCounter > tgtSettle) {
					setRunMode(SCALING);		//     If just done settling switch from settling to scaling
				}
			}
			break;
		case SCALING:							// When scaling
			if (pastCoil > maxPeak) {			//  If the peak was more than maxPeak, increase the 
				peakScale += 1;					//   scaling factor by one. We want 1 <= peaks < 2
			}
			uspb = topTime - lastTime;			//   Microseconds per beat is whatever we measured for this beat
			uspb += ((bias * uspb) + 432000) / 864000; // plus the (rounded) Arduino clock correction
			if (uspb > 5000000) {				//   If the measured beat is more than 5 seconds long
				uspb = 0;						//     it can't be real -- just ignore it
			}
			if (tick) {							//   If tick
				tickPeriod = uspb;				//     Remember tickPeriod
			} else {							//   Else (tock)
				tockPeriod = uspb;				//     Remember tockPeriod
				if (++cycleCounter > tgtScale) {
					setRunMode(CALIBRATING);	//     If just done scaling switch from scaling to calibrating
				}
			}
			break;
		case CALIBRATING:						// When calibrating
			if (tickAvg == 0 && !tick) {		//   If starting a calibration on a tock
				tick = true;					//     Swap ticks and tocks; the calculations
			}									//     assume starting on a tick
			if (tick) {							//   If tick
				tickPeriod = topTime - lastTime;//     Calculate tick period and update tick average
				tickPeriod += ((bias * tickPeriod) + 432000) / 864000;
				tickAvg += (tickPeriod - tickAvg) / curSmoothing;
			} else {							//   Else it's tock
				tockPeriod = topTime - lastTime;//     Calculate tock period and update tock average											
				tockPeriod += ((bias * tockPeriod) + 432000) / 864000;
				tockAvg += (tockPeriod - tockAvg) / curSmoothing;
				if (++curSmoothing > tgtSmoothing) {
												//     If just reached a full smoothing interval
					setRunMode(CALFINISH);		//       Switch to CALFINISH mode
				}
			}
			if (tockAvg == 0) {					//   If no tockAvg, uspb is tickAvg
				uspb = tickAvg;
			} else {							//   If both tickAvg and tockAvg, uspb is their average
				uspb = (tickAvg + tockAvg) / 2;
			}
			break;
		case CALFINISH:							// When finished calibrating
			setRunMode(RUNNING);				//  Switch to running mode
			break;
//		case RUNNING:							// When running
//												//  Nothing to do
	}
	tick = !tick;								// Switch whether a tick or a tock
	timeBeforeLast = lastTime;					// Update timeBeforeLast
	lastTime = topTime;							// Update lastTime
	return uspb;								// Return microseconds per beat
}

// Do one cycle (two beats) return length of a cycle in μs
long Bendulum::cycle() {
	return beat() + beat();						// Do two beats, return how long it took
}

/*
 *
 * Getters and setters
 *
 */
// Get the number of cycles we've been in the current mode
int Bendulum::getCycleCounter(){
	if (runMode == RUNNING) return -1;			// We don't count this since it could be huge
	if (runMode == CALIBRATING) return curSmoothing;
	return cycleCounter;
}
 
 // Set target smoothing interval in beats
int Bendulum::getTgtSmoothing(){
	return tgtSmoothing;
}
void Bendulum::setTgtSmoothing(int interval){
	tgtSmoothing = interval;
}

// Set current smoothing interval in beats
int Bendulum::getTgtSettle(){
	return tgtSettle;
}

// Set target settling interval in beats
void Bendulum::setTgtSettle(int interval){
	tgtSettle = interval;
}

// Get, set or increment Arduino clock run rate correction in tenths of a second per day
int Bendulum::getBias(){
	return bias;
}
void Bendulum::setBias(int factor){
	bias = factor;
}
int Bendulum::incrBias(int factor){
	bias += factor;
	return bias;
}

// Get/set the value of peakScale -- the scaling factor by which induced coil voltage readings is divided
int Bendulum::getPeakScale() {
	return peakScale;
}
void Bendulum::setPeakScale(int scaleFactor) {
	peakScale = scaleFactor;
}

// Was the last beat a "tick" or a "tock"?
boolean Bendulum::isTick() {
	return tick;
}

// Get average beats per minute
float Bendulum::getAvgBpm(){
	if ((tickAvg + tockAvg) == 0) return 0;
	return 120000000.0 / (tickAvg + tockAvg);
}

// Get current beats per minute
float Bendulum::getCurBpm(){
	unsigned long diff;
	if (lastTime == 0 || timeBeforeLast == 0) return 0;
	diff = lastTime - timeBeforeLast;
	return 60000000.0 / (diff + ((bias * diff) + 432000) / 864000);
}

// Get the current ratio of tick length to tock length
float Bendulum::getDelta(){
	if (tickPeriod == 0 || tockPeriod == 0) return 0;
	return (float)tickPeriod / tockPeriod;
}

// Get or set the beat duration in μs or increment it in tenths of a second per day
long Bendulum::getBeatDuration(){
	return uspb;
}
void Bendulum::setBeatDuration(long beatDur) {
	uspb = tickAvg = tockAvg = beatDur;
}
long Bendulum::incrBeatDuration(long incr) {
	long noOflow = 2000000000 / uspb;		// About the biggest adjustment it's safe to make
	while (incr > noOflow) {
		tickAvg = tockAvg = uspb += ((noOflow * uspb) + 432000) / 864000;
		incr -= noOflow;
	}
	tickAvg = tockAvg = uspb += ((incr * uspb) + 432000) / 864000;
	return uspb;
}


// Get/set the current run mode -- SETTLING, CALIBRATING or RUNNING
int Bendulum::getRunMode(){
	return runMode;
}
void Bendulum::setRunMode(byte mode){
	switch (mode) {
		case SETTLING:						//   Switch to settling mode
			runMode = SETTLING;
			cycleCounter = 1;				//     Reset cycle counter
			break;
		case SCALING:						//   Switch to scaling mode
			runMode = SCALING;
			cycleCounter = 1;				//     Reset cycle counter
			peakScale = 10;					//     Reset peakScale
			break;
		case CALIBRATING:					//   Switch to calibrating mode
			runMode = CALIBRATING;
			tickAvg = tockAvg = 0;			//     Reset averages
			curSmoothing = 1;
			break;
		case CALFINISH:						//   Switch to calibration finished mode
			runMode = CALFINISH;
			break;
		case RUNNING:						//   Switch to running mode
			runMode = RUNNING;
			break;
	}
}
