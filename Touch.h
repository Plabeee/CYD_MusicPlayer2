/*
   XPT2046 Touch Screen Controller Driver

   Concept, design and implementation by: Craig A. Lindley
   Last Update: 11/06/2025
*/

#ifndef TOUCH_H
#define TOUCH_H

#include "SoftSPI.h"

#define ROTATION_VALUE 2

#define X_TOP_LEFT 200
#define Y_TOP_LEFT 3867

#define X_TOP_RIGHT 3806
#define Y_TOP_RIGHT 3885

#define X_BOT_LEFT 170
#define Y_BOT_LEFT 338

#define X_BOT_RIGHT 3796
#define Y_BOT_RIGHT 294

typedef struct {
  int x;
  int y;
} POINT;

SoftSPI sSPI(TOUCH_MOSI, TOUCH_MISO, TOUCH_CLK);

class Touch {

public:

  // Initialize touch controller
  void begin() {

    sSPI.begin();
    sSPI.setDataMode(SPI_MODE0);

    // Setup GPIO lines
    pinMode(TOUCH_IRQ, INPUT_PULLUP);
    pinMode(TOUCH_CS, OUTPUT);
    digitalWrite(TOUCH_CS, HIGH);
    delay(10);

    // Do initial read to clear hardware
    digitalWrite(TOUCH_CS, LOW);
    sSPI.transfer(0xD0);
    sSPI.transfer(0x00);
    sSPI.transfer(0x00);
    digitalWrite(TOUCH_CS, HIGH);

    _ms = millis();
  }

  // Has screen been touched ?
  boolean touched(void) {
    return (digitalRead(TOUCH_IRQ) == LOW);
  }

  // Get the touch point or -1, -1 if none
  POINT getTouchPoint(void) {
    POINT touchPoint;

    // Assume display not touched
    touchPoint.x = -1;
    touchPoint.y = -1;

    if (touched()) {
      // Get the new touch point
      update();

      touchPoint.x = _x;
      touchPoint.y = _y;
    }
    return touchPoint;
  }

protected:
  // Protected data
  int _x, _y;
  int _rotation;
  unsigned long _ms;

  // Protected methods
  // Map touch data to screen rotation
  void doMapping(int x, int y) {
    switch (ROTATION_VALUE) {
      case 0:
        _x = map(x, X_TOP_LEFT, X_TOP_RIGHT, 0, 239);
        _y = map(y, Y_TOP_LEFT, Y_BOT_LEFT, 0, 319);
        break;

      case 1:
        _y = map(y, Y_TOP_RIGHT, Y_BOT_RIGHT, 0, 319);
        _x = map(x, X_TOP_RIGHT, X_TOP_LEFT, 0, 239);
        break;

      case 2:
        _x = map(x, X_BOT_RIGHT, X_BOT_LEFT, 0, 239);
        _y = map(y, Y_BOT_RIGHT, Y_TOP_RIGHT, 0, 319);
        break;

      case 3:
        _y = map(y, Y_BOT_LEFT, Y_TOP_LEFT, 0, 319);
        _x = map(x, X_BOT_LEFT, X_BOT_RIGHT, 0, 239);
        break;
    }
    if (_x < 0) {
      _x = 0;
    }
    if (_y < 0) {
      _y = 0;
    }
  }


  // Talk to the hardware to get newest values
  void update(void) {

    int rawX, rawY, tmp;

    unsigned long now = millis();

    // Has enough time elasped since last call
    if ((now - _ms) >= 100) {

      digitalWrite(TOUCH_CS, LOW);

      // Do sample request for x position
      sSPI.transfer(0xD0);
      tmp = (sSPI.transfer(0x00) << 5);
      tmp |= (sSPI.transfer(0x00) >> 3) & 0x1F;
      rawX = tmp;

      // Do sample request for y position
      sSPI.transfer(0x90);
      tmp = (sSPI.transfer(0x00) << 5);
      tmp |= (sSPI.transfer(0x00) >> 3) and 0x1F;
      rawY = tmp;

      digitalWrite(TOUCH_CS, HIGH);

      // Map touch data to screen rotation
      doMapping(rawX, rawY);

      // Serial.printf("RawX: %d, RawY: %d, X: %d, Y: %d\n", rawX, rawY, _x, _y);

      _ms = now;
    }
  }
};

#endif
