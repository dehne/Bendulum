# Arduino Bendulum Library

Bendulum.cpp Copyright 2013 by D. L. Ehnebuske  
License terms: [Creative Commons Attribution-ShareAlike 3.0 United States (CC BY-SA 3.0 US)]
(http://creativecommons.org/licenses/by-sa/3.0/us/ "CC BY-SA 3.0 US")

## What is a "bendulum"?

A "bendulum" is a long, slender, springy piece of metal, held vertically, whose lower end is fixed and whose upper 
end has a weight that is free to move back and forth -- a sort of upside-down pendulum. A bendulum terminates in a 
magnet that moves across a many-turn coil of fine wire positioned at the center of its swing. The motion of the magnet 
is detected as it passes over the coil by the current the magnet induces. Just after the magnet passes, a pulse of 
current through the coil induces a magnetic field in it. The field gives the magnet a small push, keeping the bendulum 
going. 

## What does this library do?

This Arduino library defines a Bendulum object that can be used to drive a bendulum or pendulum connected to and
Arduino. A Bendulum object works just as well for a pendulum whose bob has a magnet at its end.

The main method is beat(). This method is intended to be invoked repeatedly in the loop() function of an Arduino sketch.
Each time the magnet passes, beat() returns the number of microseconds that have passed since the magnet passed the
last time. In this way, the bendulum can be used to drive a time-of-day clock display.

A Bendulum object has three operational modes. The way the duration of a beat is determined depends on which mode the 
Bendulum object is in:

| Mode        | Beat duration determined by                                                                  |
|------------ | -------------------------------------------------------------------------------------------- |
| SETTLING    | The duration returned is measured using the (corrected) Arduino real-time Clock.             |
| SCALING     | The duration returned is measured using the (corrected) Arduino real-time Clock.             |
| CALIBRATING | A running average of beat duration is updated using the measured duration of the current beat. As in the SETTLING mode, the current duration is measured with the (corrected) Arduino real-time clock. The updated average is returned.|
| CALFINISH   | The duration returned is the value of the running average. No measurement is done.           |
| RUNNING     | The duration returned is the value of the running average. No measurement is done.           |

Unless setRunMode() is used to change it, when beat() is first called the Bendulum object is in SETTLING mode. 
It continues in this mode for getTgtSettle() cycles (one cycle = two beats). The purpose of this mode is to let
the bendulum or pendulum settle into a regular motion since its motion is typically disturbed at startup from 
having been given a start-up push by hand. Once it completes SETTLING the Bendulum object switches to SCALING mode 
for getTgtScale() cycles. During SCALING mode, the peak voltage induced in the coil by the passing magnet is 
determined and saved in peakScale. With SCALING over, the Bendulum object moves to CALIBRATING mode, in which it 
remains for getTgtSmoothing() additional cycles. During CALIBRATING mode, the average duration of tick and tock 
beats is measured and saved in tickAvg, tockAvg and their average -- uspb. At the end of CALIBRATING the Bendulum 
object switches to CALFINISH mode for one beat. The CALFINISH mode serves as notice to the using sketch that 
calibration is now complete. At that point the Bendulum object switches to RUNNING mode, in which it remains 
indefinitely.

The net effect is that the Bendulum object automatically characterizes the bendulum or pendulum it is driving,
first by measuring the strength of the pulses it induces in the coil and second by measuring the average duration 
of its beat. Once the measurements are done, the Bendulum object assumes the bendulum or pendulum is isochronous.

This would work nearly perfectly except that the real-time clock in most Arduinos is stable but not too accurate 
(it's a ceramic resonator, not a crystal). That is, real-time clock ticks are essentially equal to one another in 
duration but their durations are not exactly the number of microseconds they should be. To correct for this, we use 
a correction factor, getBias(). The value of getBias() is the number of tenths of a second per day by which the 
real-time clock in the Arduino must be compensated in order for it to be accurate. This number may be set using 
setBias() and incremented/decremented by using incrBias(). Positive bias means the real-time clock's "microseconds" 
are shorter than real microseconds.

In addition to just instantiating a Bendulum object and letting it go through its modes, it's possible to change the
mode as required using setRunMode(). For example, to recalibrate, simply do a setRunMode(CALIBRATING). The bendulum 
will enter CALIBRATING mode, do a calibration run and then enter CALFINISH for a beat and, finally, settle into 
RUNNING mode.

It is also possible to operate a bendulum whose parameters you know and and skip all the automatic calibration
stuff. After instantiating the Bendulum object, put it in RUNNING mode by invoking setRunMode(RUNNING) and then set 
the beat duration with setBeatDuration() and the peak scaling with setPeakScale().
