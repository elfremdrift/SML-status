#pragma once

#define MAX_TIMER_SERIAL_INSTANCES 2
#define TIMER_SERIAL_BUFFER_SIZE 64

extern "C" void TIMER2_COMPA_vect();
extern "C" void TIMER2_COMPB_vect();
extern "C" void INT0_vect();
extern "C" void INT1_vect();

class TimerSerial {
public:
  TimerSerial(byte pin, int baud, bool invert);
  ~TimerSerial();

  bool begin();
  bool end();

private:
  byte timer;
  int baudrate;
  bool invert;
  bool valid = false;
  bool enabled = false;
  byte buffer[TIMER_SERIAL_BUFFER_SIZE];
  byte* pIn = buffer;
  byte* pOut = buffer;
  byte bufCount = 0;
  byte* mapTable;
  static byte timersUsed;
  byte nextBit;
  byte nextByte;

  void readyForNext();

  void enableChange();
  void disableChange();
  void enableCompare();
  void disableCompare();

  void compareIntr();
  void changeIntr();

  
  friend void TIMER2_COMPA_vect();
  friend void TIMER2_COMPB_vect();
  friend void INT0_vect();
  friend void INT1_vect();
};
