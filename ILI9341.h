/*
  ILI9341 LCD Driver

  SPI interface is HSPI

  Craig A. Lindley
  Last Update: 11/06/2025

*/

#ifndef ILI9341_H
#define ILI9341_H

#include "SPI.h"
#include "Adafruit_GFX.h"

#define ILI9341_WIDTH      240
#define ILI9341_HEIGHT     320

#define BL_OFF             LOW
#define BL_ON              HIGH

#define ILI9341_SLPOUT     0x11 // Sleep Out
#define ILI9341_DISPON     0x29 // Display On
#define ILI9341_INVOFF     0x20 // Display Invert Off
#define ILI9341_INVON      0x21 // Display Invert On
#define ILI9341_CASET      0x2A // Column Address Set
#define ILI9341_RASET      0x2B // Row Address Set
#define ILI9341_RAMWR      0x2C // Memory Write

#define ILI9341_MADCTL     0x36 // Memory Data Access Control
#define ILI9341_MADCTL_MY  0x80
#define ILI9341_MADCTL_MX  0x40
#define ILI9341_MADCTL_MV  0x20
#define ILI9341_MADCTL_ML  0x10
#define ILI9341_MADCTL_BGR 0x08
#define ILI9341_MADCTL_MH  0x04

#define ILI9341_DEFAULT_FREQ  20000000
#define ILI9341_MAX_PIXELS_AT_ONCE  32

// ILI9341 initialization data
const uint8_t ILI9341_INIT_DATA[] = {
  0xEF, 3, 0x03, 0x80, 0x02,
  0xCF, 3, 0x00, 0XC1, 0X30,
  0xED, 4, 0x64, 0x03, 0X12, 0X81,
  0xE8, 3, 0x85, 0x00, 0x78,
  0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
  0xF7, 1, 0x20,
  0xEA, 2, 0x00, 0x00,
  0xC0, 1, 0x23,
  0xC1, 1, 0x10,
  0xC5, 2, 0x3e, 0x28,
  0xC7, 1, 0x86,
  0x36, 1, 0x48,
  0x3A, 1, 0x55,
  0xB1, 2, 0x00, 0x18,
  0xB6, 3, 0x08, 0x82, 0x27,
  0xF2, 1, 0x00,
  0x26, 1, 0x01,
  0xE0, 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
  0xE1, 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
  0x00
};

typedef struct {
  uint8_t madctl;
  uint8_t bmpctl;
  uint16_t width;
  uint16_t height;
} rotation_data_t;

const rotation_data_t ILI9341_ROTATIONS[4] = {
  {(ILI9341_MADCTL_MX | ILI9341_MADCTL_BGR), (ILI9341_MADCTL_MX | ILI9341_MADCTL_MY | ILI9341_MADCTL_BGR), ILI9341_WIDTH, ILI9341_HEIGHT},
  {(ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR), (ILI9341_MADCTL_MV | ILI9341_MADCTL_MX | ILI9341_MADCTL_BGR), ILI9341_HEIGHT, ILI9341_WIDTH},
  {(ILI9341_MADCTL_MY | ILI9341_MADCTL_BGR), (ILI9341_MADCTL_BGR), ILI9341_WIDTH, ILI9341_HEIGHT},
  {(ILI9341_MADCTL_MX | ILI9341_MADCTL_MY | ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR), (ILI9341_MADCTL_MY | ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR), ILI9341_HEIGHT, ILI9341_WIDTH}
};

// 16 bit Color definitions
#define ILI9341_BLACK       0x0000 /*   0,   0,   0 */
#define ILI9341_NAVY        0x000F /*   0,   0, 128 */
#define ILI9341_DARKGREEN   0x03E0 /*   0, 128,   0 */
#define ILI9341_DARKCYAN    0x03EF /*   0, 128, 128 */
#define ILI9341_MAROON      0x7800 /* 128,   0,   0 */
#define ILI9341_PURPLE      0x780F /* 128,   0, 128 */
#define ILI9341_OLIVE       0x7BE0 /* 128, 128,   0 */
#define ILI9341_LIGHTGREY   0xC618 /* 192, 192, 192 */
#define ILI9341_DARKGREY    0x7BEF /* 128, 128, 128 */
#define ILI9341_BLUE        0x001F /*   0,   0, 255 */
#define ILI9341_GREEN       0x07E0 /*   0, 255,   0 */
#define ILI9341_CYAN        0x07FF /*   0, 255, 255 */
#define ILI9341_RED         0xF800 /* 255,   0,   0 */
#define ILI9341_MAGENTA     0xF81F /* 255,   0, 255 */
#define ILI9341_YELLOW      0xFFE0 /* 255, 255,   0 */
#define ILI9341_WHITE       0xFFFF /* 255, 255, 255 */
#define ILI9341_ORANGE      0xFD20 /* 255, 165,   0 */
#define ILI9341_GREENYELLOW 0xAFE5 /* 173, 255,  47 */
#define ILI9341_PINK        0xF81F

class ILI9341 : public Adafruit_GFX {

  public:

    ILI9341(byte pCS, byte pRST, byte pDC, byte pBL) :
      Adafruit_GFX(ILI9341_WIDTH, ILI9341_HEIGHT) {

      // Save incoming
      _CS  = pCS;
      _RST = pRST;
      _DC  = pDC;
      _BL  = pBL;

      _width  = ILI9341_WIDTH;
      _height = ILI9341_HEIGHT;
      _freq   = ILI9341_DEFAULT_FREQ;
    }

    void begin(SPIClass *h_SPI) {
      // Save incoming
      _h_SPI = h_SPI;

      pinMode(_DC, OUTPUT);
      digitalWrite(_DC, LOW);
      pinMode(_CS, OUTPUT);
      digitalWrite(_CS, HIGH);
      pinMode(_BL, OUTPUT);
      digitalWrite(_BL, BL_OFF);

      pinMode(_RST, OUTPUT);
      digitalWrite(_RST, HIGH);
      delay(100);
      digitalWrite(_RST, LOW);
      delay(100);
      digitalWrite(_RST, HIGH);
      delay(200);

      startWrite();
      writeInitData(ILI9341_INIT_DATA);
      writeCommand(ILI9341_SLPOUT);
      delay(120);
      writeCommand(ILI9341_DISPON);
      delay(120);
      endWrite();
      digitalWrite(_BL, BL_ON);
    }

    // Control the backlight state
    void backlight(boolean state) {
      digitalWrite(_BL, state);
    }

    // Convert 24 bit RGB color to 16 bit color
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
      return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
    }

    void setTextColor(uint16_t fg, uint16_t bg) {
      Adafruit_GFX::setTextColor(fg, bg);
    }

    // Set display rotation
    void setRotation(uint8_t r) {
      uint8_t rotation = r % 4; // 0 .. 3

      uint8_t m = ILI9341_ROTATIONS[rotation].madctl;
      _width  = ILI9341_ROTATIONS[rotation].width;
      _height = ILI9341_ROTATIONS[rotation].height;

      startWrite();
      writeCommand(ILI9341_MADCTL);
      _h_SPI->write(m);
      endWrite();
    }

    int width() {
      return _width;
    }

    int height() {
      return _height;
    }

    // Control display inversion
    void invertDisplay(boolean i) {
      startWrite();
      writeCommand(i ? ILI9341_INVON : ILI9341_INVOFF);
      endWrite();
    }

    // Draw a pixel on the display
    void drawPixel(int16_t x, int16_t y, uint16_t color) {
      startWrite();
      writePixel(x, y, color);
      endWrite();
    }

    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
      startWrite();
      writeFastVLine(x, y, h, color);
      endWrite();
    }

    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
      startWrite();
      writeFastHLine(x, y, w, color);
      endWrite();
    }

    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
      startWrite();
      writeFillRect(x, y, w, h, color);
      endWrite();
    }

    void clearScreen() {
      fillRect(0, 0, _width, _height, ILI9341_BLACK);
    }

    void setTextSize(int size) {
      _size = size;
      Adafruit_GFX::setTextSize(_size);
    }

    int getTextSize(void) {
      return _size;
    }

  protected:

    // Transaction API
    void startWrite(void) {
      _h_SPI->beginTransaction(SPISettings(_freq, MSBFIRST, SPI_MODE0));
      digitalWrite(_CS, LOW);
    }

    void endWrite(void) {
      digitalWrite(_CS, HIGH);
      _h_SPI->endTransaction();
    }

    void setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
      uint32_t xa = ((uint32_t)x << 16) | (x + w - 1);
      uint32_t ya = ((uint32_t)y << 16) | (y + h - 1);
      writeCommand(ILI9341_CASET);
      _h_SPI->write32(xa);
      writeCommand(ILI9341_RASET);
      _h_SPI->write32(ya);
      writeCommand(ILI9341_RAMWR);
    }

    void writePixel(int16_t x, int16_t y, uint16_t color) {
      if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height)) return;
      setAddrWindow(x, y, 1, 1);
      _h_SPI->write16(color);
    }

    void writePixels(uint16_t *colors, uint32_t len) {
      _h_SPI->writePixels((uint8_t *) colors , len * 2);
    }

    void writeColor(uint16_t color, uint32_t len) {
      static uint16_t temp[ILI9341_MAX_PIXELS_AT_ONCE];
      size_t blen = (len > ILI9341_MAX_PIXELS_AT_ONCE) ? ILI9341_MAX_PIXELS_AT_ONCE : len;
      uint16_t tlen = 0;

      for (uint32_t t = 0; t < blen; t++) {
        temp[t] = color;
      }

      while (len) {
        tlen = (len > blen) ? blen : len;
        writePixels(temp, tlen);
        len -= tlen;
      }
    }

    void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
      if ((x >= _width) || (y >= _height)) return;

      int16_t x2 = x + w - 1, y2 = y + h - 1;
      if ((x2 < 0) || (y2 < 0)) return;

      // Clip left/top
      if (x < 0) {
        x = 0;
        w = x2 + 1;
      }
      if (y < 0) {
        y = 0;
        h = y2 + 1;
      }

      // Clip right/bottom
      if (x2 >= _width)  w = _width  - x;
      if (y2 >= _height) h = _height - y;

      int32_t len = (int32_t)w * h;
      setAddrWindow(x, y, w, h);
      writeColor(color, len);
    }

    void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
      writeFillRect(x, y, 1, h, color);
    }

    void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
      writeFillRect(x, y, w, 1, color);
    }

  private:

    // Private data
    SPIClass *_h_SPI;
    byte      _CS, _RST, _DC, _BL;
    byte      _size;
    uint32_t  _freq;

    // Private functions
    void writeInitData(const uint8_t * data) {
      uint8_t cmd, len, i;
      while (true) {
        cmd = *data++;
        if (!cmd) { //END
          return;
        }
        len = *data++;
        writeCommand(cmd);
        for (i = 0; i < len; i++) {
          _h_SPI->write(*data++);
        }
      }
    }

    void writeCommand(uint8_t cmd) {
      digitalWrite(_DC, LOW);
      _h_SPI->write(cmd);
      digitalWrite(_DC, HIGH);
    }
};

#endif
