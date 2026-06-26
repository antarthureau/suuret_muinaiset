/**
 * SerialLink.h - addressed, checksummed framing over a HardwareSerial.
 *
 * Frame:  #<addr><cmd><arg>*<XX>\n   (XX = 2-hex XOR of addr+cmd+arg)
 *
 * Pure transport: it builds one frame at a time without blocking, drops
 * malformed frames, and never discards other queued bytes. poll() returns
 * true once per complete valid frame; the caller decides what it means.
 */
#ifndef SM_SERIALLINK_H
#define SM_SERIALLINK_H

#include <Arduino.h>

struct Frame {
  char addr;
  char cmd;
  char arg[40];
};

class SerialLink {
public:
  explicit SerialLink(HardwareSerial &port) : port_(port) {}

  void begin(unsigned long baud);
  void send(char addr, char cmd, const char *arg = "");
  bool poll(Frame &out);        // non-blocking; true when a frame is ready

private:
  static uint8_t xorChk(const char *s, int n);

  HardwareSerial &port_;
  char buf_[64];
  int  idx_     = 0;
  bool inFrame_ = false;
};

#endif // SM_SERIALLINK_H
