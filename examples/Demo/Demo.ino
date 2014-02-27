/****
 *
 *   Demonstration sketch for the "Bendulum" library. Version 1.0
 *
 *   Copyright 2013 by D. L. Ehnebuske 
 *   License terms: Creative Commons Attribution-ShareAlike 3.0 United States (CC BY-SA 3.0 US) 
 *                  See http://creativecommons.org/licenses/by-sa/3.0/us/ for specifics. 
 *
 *   A "bendulum" is a long, slender, springy piece of metal, held vertically whose lower end is fixed and whose upper 
 *   end has a weight that is free to  move back and forth -- a sort of upside-down pendulum. The bendulum terminates in a 
 *   magnet that moves across a many-turn coil of fine wire positioned at the center of the swing of the pendulum or 
 *   bendulum. The motion of the magnet is detected as it passes over the coil by the current the magnet induces. Just 
 *   after the magnet passes, the code delivers a pulse of current through the coil inducing a magnetic field in it  that 
 *   gives the magnet a small push, keeping the bendulum going.
 *
 *   The main method in the bendulum class is beat() which drives the bendulum for one pass over the coil. It returns
 *   the length of time (in μs) it took since the last time the magnet passed by. The return value isn't used in this 
 *   sketch, but would be if you were building a clock using a bendulum.
 *
 *   An an aside, the same code works in the same way for a pendulum with a magnet on its end.
 *
 *   Since this is a demonstration sketch it doesn't show all of the tings you can do with the class, just the basics.
 *   For the rest you'll have to read the library code!
 *
 ****/
 
#include <Bendulum.h>                                  // Import the header so we have access to the library

Bendulum b;                                            // Instantiate a bendulum object that senses on A2 and
                                                       //   kicks on pin D12
/*
 *   Setup routine called once at power-on and at reset
 */
void setup() {
  Serial.begin(9600);                                  // Start the serial monitor
  Serial.println("Bendulum Example v 1.0");            // Say who's talking on it
}

/*
 *   Loop routine called over and over so long as the Arduino is running
 */
void loop() {
  b.beat();                                            // Have the bendulum do one pass over the coil
  if (b.isTick()) {                                    // If it passed in the "tick" direction (as oppoed to "tock")
    switch (b.getRunMode()) {                          //   Do things based on the current "run mode"
      case SETTLING:                                   //   Case "settling in"
        Serial.print("Settling. Settle count: ");      //     Say that wer're settling in and how far along we've gotten
        Serial.print(b.getCurSettle());
        break;
      case CALIBRATING:                                //   Case "calibrating"
        Serial.print("Calibrating. Smoothing: ");      //     Say we're calibrating, how much smoothing We've been
        Serial.print(b.getCurSmoothing());             //       able to do so far, how symmetrical the "ticks" and
        Serial.print(", delta: ");                     //       "tocks" are currently, and how many beats per minute
        Serial.print(b.getDelta(), 4);                 //       on average we're seeing so far
        Serial.print(", Average BPM: ");
        Serial.print(b.getBpm(), 4);
        break;
      case RUNNING:                                    //   Case "running"
        Serial.print("Running. Calibrated: ");         //     Say that we're running along normally and what the
        Serial.print(b.getAvgBpm(), 4);                //       calibrated beats per minute is and what the currently
        Serial.print("(bpm), Current: ");              //       measured bpm is
        Serial.print(b.getCurBpm());
        Serial.print("(bpm)");
        break;
    }
    Serial.println(".");
  }
}
