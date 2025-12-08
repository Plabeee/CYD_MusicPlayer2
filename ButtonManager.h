/*
   On Screen Button Manager Class

   Concept, design and implementation by: Craig A. Lindley
   Last Update: 12/19/2024
*/

#ifndef BUTTONMANAGER_H
#define BUTTONMANAGER_H

// On screen touch button attributes
#define BUTTON_WIDTH          54
#define BUTTON_HEIGHT         30
#define BUTTON_OUTLINE_COLOR  ILI9341_WHITE
#define BUTTON_FILL_COLOR     ILI9341_GREEN
#define BUTTON_TEXT_COLOR     ILI9341_BLACK
#define BUTTON_TEXT_SIZE      2

#define BUTTON_SURROUND_Y     218
#define BUTTON_SURROUND_COLOR ILI9341_BLUE

// Button IDs
enum BUTTON_STATE {
  BS_NONE,
  BS_MINUS, BS_MINUSP, BS_MINUSPP,
  BS_PLUS, BS_PLUSP, BS_PLUSPP,
  BS_SELECT, BS_SELECTP, BS_SELECTPP,
  BS_BACK, BS_BACKP, BS_BACKPP,
  BS_TOUCHED, BS_TOUCHEDP, BS_TOUCHEDPP
};

// Instantiate the required buttons
static Adafruit_GFX_Button minusButton;
static Adafruit_GFX_Button plusButton;
static Adafruit_GFX_Button selectButton;
static Adafruit_GFX_Button backButton;

static MultiButton minusSB;
static MultiButton plusSB;
static MultiButton selectSB;
static MultiButton backSB;
static MultiButton touchedSB;

class ButtonManager {

  public:

    // Class Constructor
    // Parameterize the navigation buttons
    // This does not display them
    ButtonManager(ILI9341 *pLcd, Touch *pTouch) {

      // Save incoming
      _pLcd = pLcd;
      _pTouch = pTouch;

      int bX = (_pLcd->width() / 2) - (BUTTON_WIDTH / 2);
      int bY = 225;

      minusButton.initButtonUL(_pLcd, 13, bY, BUTTON_WIDTH, BUTTON_HEIGHT,
                               BUTTON_OUTLINE_COLOR, BUTTON_FILL_COLOR, BUTTON_TEXT_COLOR,
                               (char *) "-", BUTTON_TEXT_SIZE);
      backButton.initButtonUL(_pLcd, bX, bY, BUTTON_WIDTH, BUTTON_HEIGHT,
                              BUTTON_OUTLINE_COLOR, BUTTON_FILL_COLOR, BUTTON_TEXT_COLOR,
                              (char *) "Back", BUTTON_TEXT_SIZE);
      plusButton.initButtonUL(_pLcd, 173, bY, BUTTON_WIDTH, BUTTON_HEIGHT,
                              BUTTON_OUTLINE_COLOR, BUTTON_FILL_COLOR, BUTTON_TEXT_COLOR,
                              (char *) "+", BUTTON_TEXT_SIZE);
      selectButton.initButtonUL(_pLcd, bX, 281, BUTTON_WIDTH, BUTTON_HEIGHT,
                                BUTTON_OUTLINE_COLOR, BUTTON_FILL_COLOR, BUTTON_TEXT_COLOR,
                                (char *) "Sel", BUTTON_TEXT_SIZE);
    }

    // Draw the buttons on the screen
    void drawButtons(void) {

      // Draw round rect around buttons
      _pLcd->fillRoundRect(7, BUTTON_SURROUND_Y, 227, 97, 12, BUTTON_SURROUND_COLOR);

      minusButton.drawButton();
      selectButton.drawButton();
      plusButton.drawButton();
      backButton.drawButton();
    }

    void update() {

      // Get touch point
      POINT pt = _pTouch->getTouchPoint();

      // See if the touch was on a button
      minusSB.update(minusButton.contains(pt.x, pt.y));
      plusSB.update(plusButton.contains(pt.x, pt.y));
      selectSB.update(selectButton.contains(pt.x, pt.y));
      backSB.update(backButton.contains(pt.x, pt.y));

      // If the screen was touched above where the buttons are
      // located update the touched SB.
      touchedSB.update((pt.y > -1) && (pt.y < BUTTON_SURROUND_Y));
    }

    // Get button status
    enum BUTTON_STATE pollButtons() {

      if (minusSB.isSingleClick()) {
        return BS_MINUS;
      } else if (minusSB.isDoubleClick()) {
        return BS_MINUSP;
      } else if (minusSB.isLongClick()) {
        return BS_MINUSPP;

      } else if (plusSB.isSingleClick()) {
        return BS_PLUS;
      } else if (plusSB.isDoubleClick()) {
        return BS_PLUSP;
      } else if (plusSB.isLongClick()) {
        return BS_PLUSPP;

      } else if (selectSB.isSingleClick()) {
        return BS_SELECT;
      } else if (selectSB.isDoubleClick()) {
        return BS_SELECTP;
      } else if (selectSB.isLongClick()) {
        return BS_SELECTPP;

      } else if (backSB.isSingleClick()) {
        return BS_BACK;
      } else if (backSB.isDoubleClick()) {
        return BS_BACKP;
      } else if (backSB.isLongClick()) {
        return BS_BACKPP;

      } else if (touchedSB.isSingleClick()) {
        return BS_TOUCHED;
      } else if (touchedSB.isDoubleClick()) {
        return BS_TOUCHEDP;
      } else if (touchedSB.isLongClick()) {
        return BS_TOUCHEDPP;
      } else {
        return BS_NONE;
      }
    }

  protected:
    ILI9341 *_pLcd;
    Touch *_pTouch;
};

#endif
