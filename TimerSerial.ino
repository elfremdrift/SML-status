#include "TimerSerial.hpp"

byte TimerSerial::timersUsed = 0; // Bitmask of timers in use (bit 0/1, all in physical timer 2)

#define BPOS(baud, pos) (F_CPU/64*2*pos/baud)  // Calculate relative timer count for each bit to sample (pos is half bit steps)

static const byte BitPositions9600[10] PROGMEM = { BPOS(9600, 1), BPOS(9600, 3), BPOS(9600, 5), BPOS(9600, 7), BPOS(9600, 9), BPOS(9600, 11), BPOS(9600, 13), BPOS(9600, 15), BPOS(9600, 17), BPOS(9600, 19) };

static TimerSerial* currentSerial0 = nullptr;
static TimerSerial* currentSerial1 = nullptr;

TimerSerial::TimerSerial(byte pin, int baud, bool invert) : baudrate(baud), invert(invert)
{
  static bool initialized = false;
  if (!initialized) {
    // Set up timer 2:
    TIMSK2 = 0;  // No interrupts so far
    TCCR2A = 0;
    TCCR2B = _BV(CS22);  // Factor 64 prescaler
    initialized = true;
  }
  if (pin != 2 && pin != 3) {  // Need to use pin 2=IN0 or 3=IN1
    return;
  }
  timer = pin - 2;
  if (timersUsed & (1<<timer)) {
    return;  // Already in use
  }
  switch (baud) {
  case 9600:
    mapTable = BitPositions9600;
    break;
  default:
    return;  // Unsupported baud rate
  }
  // No conflicts, set valid and reserve timer/pin
  disableChange();
  disableCompare();
  timersUsed |= (1<<timer);
  valid = true;
  (timer == 0 ? currentSerial0 : currentSerial1 ) = this;
  // Set up correct INT0/1 sense:
  EICRA = (EICRA & (0xf0 + (timer==0 ? (_BV(ISC11)|_BV(ISC10)):(_BV(ISC11)|_BV(ISC10))))) |
            (timer == 0 ? ((invert?_BV(ISC00):0)|_BV(ISC01)) : ((invert?_BV(ISC10):0)|_BV(ISC11)));
}

TimerSerial::~TimerSerial()
{
  if (valid) {
    end();
    timersUsed &= ~(1<<timer);
    (timer == 0 ? currentSerial0 : currentSerial1 ) = nullptr;
  }
}

void TimerSerial::enableChange()
{
  EIMSK = EIMSK | (timer==0 ? _BV(INT0) : _BV(INT1));
}

void TimerSerial::disableChange()
{
  EIMSK = EIMSK & ~(timer==0 ? _BV(INT0) : _BV(INT1));
}

void TimerSerial::enableCompare()
{
  (timer==0 ? OCR2A : OCR2B) = TCNT2 + pgm_read_byte(mapTable + nextBit);
  TIMSK2 = TIMSK2 | (timer == 0 ? OCIE2A : OCIE2B);
}

void TimerSerial::disableCompare()
{
  TIMSK2 = TIMSK2 & ~(timer == 0 ? OCIE2A : OCIE2B);
}

void TimerSerial::readyForNext()
{
  disableCompare();
  enableChange();
}

bool TimerSerial::begin()
{
  if (!valid || enabled) return false;
  readyForNext();
  enabled = true;
}

bool TimerSerial::end()
{
  if (!enabled) return false;
  disableChange();
  disableCompare();
  enabled = false;
}

void TimerSerial::compareIntr()
{
  bool bit = (PIND & (timer==0 ? _BV(PIND2) : _BV(PIND3))) != 0;
  if (invert) bit = !bit;
  
  if (nextBit == 0) {
    // Start bit, must be false
    if (bit) {
      // Invalid noise - go back to detecting start bit flange
      disableCompare();
      enableChange();
      return;
    }
    ++nextBit;
    enableCompare();
  }
  else if (nextBit <= 8) {
    nextByte |= (bit << (nextBit-1));
    ++nextBit;
    enableCompare();
  }
  else if (nextBit == 9) {
    if (bit) {
      // Correct stop bit (true) - add to buffer if there's room
      if (bufCount < TIMER_SERIAL_BUFFER_SIZE) {
        *pIn++ = nextByte;
        ++bufCount;
        if (pIn - buffer >= TIMER_SERIAL_BUFFER_SIZE) pIn = buffer;
      }
    }
    // Set up reading next byte:
    disableCompare();
    enableChange();
  }
}

void TimerSerial::changeIntr()
{
  // Detected a change - set up timer
  nextBit = 0;
  nextByte = 0;
  disableChange();
  enableCompare();
}



ISR(TIMER2_COMPA_vect)
{
  if (currentSerial0 && currentSerial0->enabled) currentSerial0->compareIntr();
}

ISR(TIMER2_COMPB_vect)
{
  if (currentSerial1 && currentSerial1->enabled) currentSerial1->compareIntr();
}

ISR(INT0_vect)
{
  if (currentSerial0 && currentSerial0->enabled) currentSerial0->changeIntr();
}

ISR(INT1_vect)
{
  if (currentSerial1 && currentSerial1->enabled) currentSerial1->changeIntr();
}
