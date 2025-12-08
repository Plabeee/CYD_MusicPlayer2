/*
   GPIO pin definitions for the ESP32 Touch BT Music Player
   Built with the CYD

   These definitions connect the hardware to the software

   Concept, design and implementation by: Craig A. Lindley
   Last Update: 11/06/2025
*/

#ifndef HARDWARE_H
#define HARDWARE_H

// ILI9341 LCD display pins using HSPI interface
#define LCD_MOSI    13
#define LCD_MISO    12
#define LCD_SCLK    14
#define LCD_CS      15
#define LCD_RST     -1
#define LCD_DC       2
#define LCD_BL      21

// XPT2046 touch controller pins
#define TOUCH_IRQ   36
#define TOUCH_MOSI  32
#define TOUCH_MISO  39
#define TOUCH_CLK   25
#define TOUCH_CS    33

// SD card interface pin using VSPI interface
#define SD_MOSI     23
#define SD_MISO     19        
#define SD_SCK      18
#define SD_CS        5

#endif
