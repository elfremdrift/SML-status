#pragma once

#define MAX_TIMER_SERIAL_INSTANCES 2
#define TIMER_SERIAL_BUFFER_SIZE 64

class TimerSerial {
public:
  TimerSerial(int pin, int baud);

  bool valid() const;

private:
  byte timer;
  int baudrate;
  bool isValid = false;
  byte buffer[TIMER_SERIAL_BUFFER_SIZE];
  byte* pIn = buffer;
  byte* pOut = buffer;
  byte bufCount = 0;
  byte* mapTable;
  static byte timersUsed;
};
