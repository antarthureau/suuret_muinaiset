/**
 * SerialLink.cpp - frame assembly and parsing.
 * See SerialLink.h for the frame format and why the link is addressed.
 */
#include "SerialLink.h"
#include <string.h>
#include <stdlib.h>

uint8_t SerialLink::xorChk(const char *s, int n) {
  uint8_t c = 0;
  for (int i = 0; i < n; i++) c ^= (uint8_t)s[i];
  return c;
}

void SerialLink::begin(unsigned long baud) {
  port_.begin(baud);
}

void SerialLink::send(char addr, char cmd, const char *arg) {
  /*
   * Build the body first so the checksum covers exactly the bytes the receiver
   * will check. An over-long argument drops the whole frame rather than
   * sending a truncated one that would still pass its own checksum.
   */
  char body[48];
  int n = snprintf(body, sizeof(body), "%c%c%s", addr, cmd, arg);
  if (n < 0 || n >= (int)sizeof(body)) return;

  uint8_t c = xorChk(body, n);
  char out[64];
  int m = snprintf(out, sizeof(out), "#%s*%02X\n", body, c);
  if (m > 0) port_.write((const uint8_t *)out, m);
}

bool SerialLink::poll(Frame &out) {
  while (port_.available()) {
    char ch = (char)port_.read();

    /* '#' always restarts the frame, so noise cannot wedge the parser. */
    if (ch == '#') { inFrame_ = true; idx_ = 0; continue; }
    if (!inFrame_) continue;

    if (ch == '\n') {
      buf_[idx_] = 0;
      inFrame_ = false;

      char *star = strchr(buf_, '*');
      int blen = star ? (int)(star - buf_) : -1;
      idx_ = 0;

      if (blen < 2) continue;                    // no room for addr + cmd

      uint8_t want = (uint8_t)strtol(star + 1, nullptr, 16);
      if (xorChk(buf_, blen) != want) continue;  // corrupted, drop

      out.addr = buf_[0];
      out.cmd  = buf_[1];

      /* Remainder is the argument, clamped to the Frame buffer. */
      int al = blen - 2;
      if (al < 0) al = 0;
      if (al > (int)sizeof(out.arg) - 1) al = sizeof(out.arg) - 1;
      memcpy(out.arg, buf_ + 2, al);
      out.arg[al] = 0;

      return true;      // leave any further bytes queued for the next call
    }

    if (idx_ < (int)sizeof(buf_) - 1) buf_[idx_++] = ch;
    else { inFrame_ = false; idx_ = 0; }         // overflow, abandon the frame
  }
  return false;
}
