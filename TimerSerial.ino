#include "TimerSerial.hpp"

byte TimerSerial::timersUsed = 0; // Bitmask of timers in use (bit 0/1, all in physical timer 2)

#define BPOS(baud, pos) (F_CPU/64/2*pos/baud)  // Calculate relative timer count for each bit to sample (pos is half bit steps)

static const byte BitPositions9600[10] PROGMEM = { BPOS(9600, 1), BPOS(9600, 3), BPOS(9600, 5), BPOS(9600, 7), BPOS(9600, 9), BPOS(9600, 11), BPOS(9600, 13), BPOS(9600, 15), BPOS(9600, 17), BPOS(9600, 19) };


TimerSerial::TimerSerial(int pin, int baud)
{
  static bool initialized = false;
  if (!initialized) {
    // Set up timer 2:
    TIMSK2 = 0;  // No interrupts so far
    TCCR2A = 0;
    TCCR2B = _BV(CS22);  // Factor 64 prescaler
    initialized = true;
  }
  for (timer = 0; timer < MAX_TIMER_SERIAL_INSTANCES; ++timer) {
    if (!(timersUsed & (1<<timer))) break;
  }
  if (timer == MAX_TIMER_SERIAL_INSTANCES) {
    // No timers available; return as invalid
    return;
  }
  switch (baud) {
  case 9600: mapTable = BitPositions9600; break;
  default:
    return;  // Unsupported baud rate
  }
}
