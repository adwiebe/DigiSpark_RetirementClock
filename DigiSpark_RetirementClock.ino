//===============================================
//
// February 2023
//
// Use DigiSpark 16MHz setting in Arduino 1.8.8
//
//===============================================

#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>

#define LED  1

//8760 too long? - Jan 31,2023
// 8750 is giving me between 10k & 11k
// Seems going to sleep and waking up are not instantaneous. And there's time pulsing the heart beat.
// (8760/10500)*8760 = 7308
// Set to 7300 - Feb 1, 2023

// Running too fast.
// Averaging some test points suggests 8,500
// Set to 8,250 - Feb 2, 2023

// Still too fast - tests reinforce the 8,500
// Set to 8,500 - Feb 3, 2023

// Too slow. 8466 - Feb 6, 2023
// 8464 to be /8

#define TICK_SECONDS  8464
#define SLEEP_TIME    8

byte saveADCSRA;
volatile long counterWD = 0;

//=============================================================================================//
//                                                                                             //
// TO UPDATE UNPLUG SPARK, RUN UPDATE, THEN PLUG IN WHEN CONSOLE SAYS "Plug in device now..."  //
//                                                                                             //
//=============================================================================================//

// Alertnate between 2 pins to run the bipolar stepper in a cheap battery clock
// The clock has been hacked to run backwards (the metal bar flipped - because magic)
//
// 3600 seconds in an hour. But I want an hour to take a year. Or 365 days.
// 
// A year is 3600 (s/hr) * 24 (h/d) * 365 d = 31,536,000 seconds.
// 31,536,000 (s/yr) / 3,600 (ticks / year) = 8,760 seconds per tick.
// (~2.43 hours per tick - very slow clock...)
//
// Sanity check.
// 5 minutes (moving from 3 to 2 for example) will be 1 month (there are 12 x 5 min intevals / hour)
// 5 minutes is 300 ticks (seconds).
// 300 = 8,760 = 2,628,000 seconds.
// 2,628,000 seconds / 3600 (s/h) / 24 (h/d) = 30.4 days
// 30.4 days * 12 months = 365 days.
//
// Ignore leap years - clock will need battery changes, etc. So period recalibration to be expected.
//
// Figuring 365.25 days / year, the pulses are 8,766 seconds apart.
//
// The clock coil needs an H-Bridge pulses for 0.032 seconds at 1.2V (max 0.010 A)
// Each pulse needs to reverse direction. +1.28V, -1.28V, +1.28V, -1.28V....
//
// Actually, looks like the ATTiny can source/sink 10mA from each pin. Leaving the coil attached to
// the original circuit may be also helping squelch the back EMF as a scope does not show any pulse
// on the trailing edge. So circuit is ridiculously simple. Just P0-{coil}-P2 (P1 is internal LED which
// may have been causing stability issues - repurposed as a heartbeat)
//
// So each pulse needs to alternate between 2 pins.
// PinPluss
// PinNegative
//

void Pulse() {
  static int p = 0;
  digitalWrite(2*p, HIGH);
  delay(32);
  digitalWrite(2*p, LOW);
  p = !p; // Toggle 
}

void PulseLed() {
  digitalWrite(LED,HIGH);
  delay(10);
  digitalWrite(LED,LOW);
}

void sleepNow ()
{
  set_sleep_mode ( SLEEP_MODE_PWR_DOWN ); // set sleep mode Power Down
  saveADCSRA = ADCSRA;                    // save the state of the ADC. We can either restore it or leave it turned off.
  ADCSRA = 0;                             // turn off the ADC
  power_all_disable ();                   // turn power off to ADC, TIMER 1 and 2, Serial Interface
  
  noInterrupts ();                        // turn off interrupts as a precaution
  resetWatchDog ();                       // reset the WatchDog before beddy bies
  sleep_enable ();                        // allows the system to be commanded to sleep
  interrupts ();                          // turn on interrupts
  
  sleep_cpu ();                           // send the system to sleep, night night!

  sleep_disable ();                       // after ISR fires, return to here and disable sleep
  power_all_enable ();                    // turn on power to ADC, TIMER1 and 2, Serial Interface
  
  // ADCSRA = saveADCSRA;                 // turn on and restore the ADC if needed. Commented out, not needed.
  
} // end of sleepNow ()

void resetWatchDog ()
{
  MCUSR = 0;
  WDTCR = bit ( WDCE ) | bit ( WDE ) | bit ( WDIF ); // allow changes, disable reset, clear existing interrupt
  
  // the bit<<flag notation is actually a little bit nicer for dev cycles, as you can leave all flags in the code and 'toggle' the bit you want.
  
  // IF CHANGING THE SLEEP TIME BITS MAKE SURE TO CHANGE THE #define ABOVE!!
  WDTCR = 1<<WDIE | 1<<WDP3 | 0<<WDP2 | 0<<WDP1 | 1<<WDP0 ; // Enable interrupt, 8 seconds (see ATTINY datasheet)
//  WDTCR = 1<<WDIE | 1<<WDP3 | 0<<WDP2 | 0<<WDP1 | 0<<WDP0 ; // Enable interrupt, 4 seconds (see ATTINY datasheet)
//  WDTCR = 1<<WDIE | 0<<WDP3 | 1<<WDP2 | 1<<WDP1 | 1<<WDP0 ; // Enable interrupt, 2 seconds (see ATTINY datasheet)
//  WDTCR = 1<<WDIE | 0<<WDP3 | 1<<WDP2 | 1<<WDP1 | 0<<WDP0 ; // Enable interrupt, 1 seconds (see ATTINY datasheet)
    
  wdt_reset ();                            // reset WDog to parameters
  
} // end of resetWatchDog ()

ISR ( WDT_vect )
{
  wdt_disable ();                           // until next time....
  counterWD ++;
} // end of ISR


// the setup routine runs once when you press reset:
void setup() {                
  pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);
  digitalWrite(0,LOW);
  digitalWrite(1,LOW);
  digitalWrite(2,LOW);

  // Drive the clock 1 step to get the stepper in sync.
  Pulse();
}

// the loop routine runs over and over again forever:
void loop() {

  PulseLed(); // Visible Heartbeat
  if (counterWD > (TICK_SECONDS / SLEEP_TIME)){
    Pulse();
    counterWD = 0;
  }
  sleepNow();

}
