/**
 * SerialLink.h - addressed, checksummed framing over a HardwareSerial port.
 *
 * ---------------------------------------------------------------------------
 * FRAME FORMAT
 * ---------------------------------------------------------------------------
 *   #<addr><cmd><arg>*<XX>\n
 *
 *   #     start of frame; resynchronizes the parser wherever it appears
 *   addr  '0' all followers, '1' SMALL, '2' SEASHELL, 'L' leader
 *   cmd   single character, see the proto namespace in config.h
 *   arg   optional payload, may be empty
 *   *XX   two hex digits, XOR of every byte of addr + cmd + arg
 *   \n    end of frame
 *
 * ---------------------------------------------------------------------------
 * WHY ADDRESSED
 * ---------------------------------------------------------------------------
 * All three units share one half-duplex pair over underground CAT6: the
 * leader's TX reaches both followers, and both followers' TX return on the
 * leader's RX. Nothing arbitrates that shared back-channel, so collisions are
 * avoided by policy instead - only the unit a frame is addressed to may
 * answer, and the leader staggers its two status requests in time so the
 * followers cannot reply at once.
 *
 * This replaced the V1 approach of calling Serial3.end() and begin() to force
 * direction, which discarded queued bytes and lost frames.
 *
 * ---------------------------------------------------------------------------
 * ROBUSTNESS
 * ---------------------------------------------------------------------------
 * The parser is a byte-at-a-time state machine that never blocks and never
 * waits for a frame to complete. Corrupt, truncated and oversized frames are
 * dropped silently, and a stray '#' restarts the frame rather than poisoning
 * the parser - which matters on a long buried cable where electrical noise is
 * expected. Bytes that arrive after a complete frame stay in the UART FIFO for
 * the next call, so nothing good is ever discarded to get at something else.
 *
 * This class is pure transport. It assigns no meaning to addresses or
 * commands; the Controller decides what a frame means and whether it applies.
 */
#ifndef SM_SERIALLINK_H
#define SM_SERIALLINK_H

#include <Arduino.h>

/** One decoded frame. arg is always NUL-terminated, possibly empty. */
struct Frame {
  char addr;
  char cmd;
  char arg[40];
};

class SerialLink {
public:
  explicit SerialLink(HardwareSerial &port) : port_(port) {}

  void begin(unsigned long baud);

  /** Build and transmit one frame. Oversized frames are dropped, not truncated. */
  void send(char addr, char cmd, const char *arg = "");

  /**
   * Non-blocking receive. Returns true once per complete, checksum-valid
   * frame, leaving any further buffered bytes for the next call. Call it in a
   * loop until it returns false to drain a burst.
   */
  bool poll(Frame &out);

private:
  static uint8_t xorChk(const char *s, int n);

  HardwareSerial &port_;
  char buf_[64];        // frame body being assembled, excluding '#' and '\n'
  int  idx_     = 0;
  bool inFrame_ = false;
};

#endif  // SM_SERIALLINK_H
