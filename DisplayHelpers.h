/*
   Display Helper Class

   Concept, design and implementation by: Craig A. Lindley
   Last Update: 11/06/2025
*/

#ifndef DISPLAYHELPER_H
#define DISPLAYHELPER_H

#include "ILI9341.h"

class DisplayHelper : public ILI9341 {

  public:

    DisplayHelper(byte pCS, byte pRST, byte pDC, byte pBL) :
      ILI9341(pCS, pRST, pDC, pBL) {
    }

    // Get the width in pixels of a text string in current font
    uint16_t getTextWidth(const char *text) {

      int16_t x1, y1;
      uint16_t w, h;

      getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

      return w;
    }

    // Get the height in pixels of a text string in current font
    uint16_t getTextHeight(const char *text) {

      int16_t x1, y1;
      uint16_t w, h;

      getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

      return h;
    }

    // Calculate the x offset necessary to center a text string
    int xOffsetToCenterText(const char *text) {
      return (width() - getTextWidth(text)) / 2;
    }

    // Draw a text string at specified location
    void drawText(int x, int y, const char *text) {
      setCursor(x, y);
      print(text);
    }

    // Draw a text string centered on display with y position
    void drawCenteredText(int y, const char *text) {
      drawText(xOffsetToCenterText(text), y, text);
    }
};

#endif
