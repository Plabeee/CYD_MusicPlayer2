/*
  ESP32 CYD Bluetooth MP3 Music Player

  This is version 2.X of this program which uses more modern
  libraries for Bluetooth connection and MP3 decodes. Version 1.X
  of this program could not be compiled and run with new versions
  of the Arduino code because of lack of available RAM.

  Software:
    This code is based upon three libraries written by Phil Schatzmann:
    They are:
      1. ESP32-A2DP
      2. arduino-audio-tools
      3. arduino-libhelix

  Hardware:
    ESP32 Cheap Yellow Display (CYD) module (ESP32-2432S028)
    SD memory card to hold the MP3 music files

  See Hardware.h for signal connections

  From the IDE's Tools menu, select:
    Board: ESP32 Dev Module
    Partition Scheme: Huge APP
    Upload Speed: 460800

  See the README.txt file for program configuration and operation

  Concept, Design and Implementation: Craig A. Lindley
  Last Update: 11/06/2025
*/

#include <SPI.h>
#include <SdFat.h>

#include <Adafruit_GFX.h>

#include "Secrets.h"

#include "SongManager.h"
#include "BluetoothA2DPSource.h"
#include "Hardware.h"
#include "DisplayHelpers.h"
#include "Touch.h"
#include "MultiButton.h"
#include "ButtonManager.h"
#include "ListBox.h"
#include "FTPUploader.h"

// Application title
#define APP_TITLE "CYD BT Music Player 2"

// Misc screen attributes
#define SCREEN_ROTATION 0
#define SCREEN_COLOR ILI9341_BLACK
#define SCREEN_BORDER_COLOR ILI9341_BLUE
#define SCREEN_TITLE_COLOR ILI9341_GREEN
#define SCREEN_TEXT_COLOR ILI9341_WHITE

// Listbox attributes
#define LISTBOX_RECT_X 2
#define LISTBOX_RECT_Y 10
#define LISTBOX_CONTENT_START LISTBOX_RECT_Y + 6
#define LISTBOX_RECT_WIDTH 236
#define LISTBOX_RECT_HEIGHT 208
#define LISTBOX_LINES 10
#define LISTBOX_CHARS 19

// Timeout for LCD display
#define DISPLAY_TIMEOUT_MIN 1
#define DISPLAY_TIMEOUT_MS (DISPLAY_TIMEOUT_MIN * 60 * 1000)

// Duration of info screen display
#define INFO_SCREEN_DELAY_MS 1500

// Program version numbers
#define MAJOR_VERSION 2
#define MINOR_VERSION 2

// Global instances of SdFat constants and variables
SdFat32 sd;
File32 file;

#define USE_SDFAT 1
#define SD_CONFIG SdSpiConfig(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(12))

/****************************************************************/
/***                 Driver Object Instantiation              ***/
/****************************************************************/

// Configure HSPI interface for LCD
SPIClass h_SPI(HSPI);

// Instantiate the LCD driver
DisplayHelper lcd(LCD_CS, LCD_RST, LCD_DC, LCD_BL);

// Instantiate the touch screen driver
Touch touch;

// Instantiate the button manager
ButtonManager bm(&lcd, &touch);

// Pointer to ListBox instance
ListBox *listBox;

// Create FTPUploader instance
FTPUploader ftpUploader;

// Create SongManager instance
SongManager songManager;

/****************************************************************/
/***                       Misc Variables                     ***/
/****************************************************************/

uint32_t displayTimeout;

boolean playing;
boolean looping;
boolean skipInput;
boolean uploading;

enum PLAY_MODE { SEQUENTIAL,
                 RANDOM };
enum PLAY_MODE playMode;

// Buffer for building paths to song files on SD card
char songPath[120];

// Finite State Machine (FSM) states
enum STATES {
  INITIAL,

  // Operation states
  OP_POPULATE_LB,
  OP_BUTTON_CHECK,
  OP_DISPATCH,

  // Bluetooth states
  BT_START,
  BT_CONNECT_WAIT,

  // Artist states
  AR_POPULATE_LB,
  AR_BUTTON_CHECK,

  // Album states
  AL_POPULATE_LB,
  AL_BUTTON_CHECK,

  // Song states
  SG_POPULATE_LB,
  SG_BUTTON_CHECK,
  SG_PLAY,
  SG_SONGSTATUS_CHECK,
  SG_PATH_RESET,

  // Action states
  AC_DISPLAY,
  AC_BUTTON_CHECK,

  // Shuffle states
  SH_PICKANDPLAY,
  SH_BUTTONSTATUS_CHECK,

  // Remote access states
  RA_WIFI_CONNECT,
  RA_DISPLAY,
  RA_BUTTON_CHECK,
};

// Set the initial start state
STATES state = INITIAL;

/****************************************************************/
/***                       Misc Functions                     ***/
/****************************************************************/

// Determine if a char strings starts with specified prefix
boolean startsWith(const char *pre, const char *str) {

  size_t lenpre = strlen(pre),
         lenstr = strlen(str);
  return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

// Gather up all the artist names into string vector
// This only needs to be done once because it doesn't change
boolean populateArtistsVector() {

  char NAME_BUFFER[128];

  // Clear any previous data
  artists.clear();

  // Open the root directory of SD card
  File root = sd.open("/");
  if (!root) {
    Serial.println("\nFailed to open directory\n\n");
    return false;
  }

  // Populate listbox with directory/artist names
  File file = root.openNextFile();
  while (file) {
    file.getName(NAME_BUFFER, sizeof(NAME_BUFFER));

    if (file.isDirectory() && !startsWith(".", NAME_BUFFER)) {
      artists.push_back(NAME_BUFFER);
    }
    file.close();
    file = root.openNextFile();
  }
  // Close the files
  file.close();
  root.close();

  // Sort the artists
  std::sort(artists.begin(), artists.end());

  return true;
}

// Gather up all the artist albums into string vector
// artistPath is "/artist1"
boolean populateArtistAlbumsVector(char *artistPath) {

  char NAME_BUFFER[50];

  // Clear any previous data
  albums.clear();

  // Open the root directory of SD card
  File root = sd.open(artistPath);
  if (!root) {
    Serial.println("\nFailed to open directory\n\n");
    return false;
  }

  // Populate vector with directory/artist/album names
  File file = root.openNextFile();
  while (file) {
    file.getName(NAME_BUFFER, sizeof(NAME_BUFFER));
    if (file.isDirectory() && !startsWith(".", NAME_BUFFER)) {
      albums.push_back(NAME_BUFFER);
    }
    file.close();
    file = root.openNextFile();
  }
  // Close the files
  file.close();
  root.close();

  // Sort the albums
  std::sort(albums.begin(), albums.end());

  return true;
}

// Gather up all the artist album songs into string vector
// albumPath is "/artist1/album1"
boolean populateArtistAlbumSongsVector(char *albumPath) {

  char NAME_BUFFER[50];

  // Clear any previous data
  songs.clear();

  // Open the root directory of SD card
  File root = sd.open(albumPath);
  if (!root) {
    Serial.println("\nFailed to open directory\n\n");
    return false;
  }

  // Populate vector with directory/artist names
  File file = root.openNextFile();
  while (file) {
    file.getName(NAME_BUFFER, sizeof(NAME_BUFFER));
    if (!file.isDirectory() && !startsWith(".", NAME_BUFFER)) {
      songs.push_back(NAME_BUFFER);
    }
    file.close();
    file = root.openNextFile();
  }
  // Close the files
  file.close();
  root.close();

  // Sort the songs
  std::sort(songs.begin(), songs.end());

  return true;
}

// Optional logging function
void audio_info(const char *info) {
  // Serial.println(info);
}

// Calculate y pixel position corresponding to a text line
int calcLineOffset(int line) {
  return (line * 30) + 25;
}

// Clear the list box area of screen
void clearListboxArea(void) {

  // Draw rectangular outline
  lcd.drawRoundRect(0, 0, lcd.width(), lcd.height(), 10, SCREEN_BORDER_COLOR);

  // Clear listbox on screen area
  lcd.fillRect(LISTBOX_RECT_X, LISTBOX_RECT_Y,
               LISTBOX_RECT_WIDTH, LISTBOX_RECT_HEIGHT, SCREEN_COLOR);

  // Draw the program's title although it should be OK
  lcd.setTextSize(1);
  lcd.drawCenteredText(4, APP_TITLE);
  lcd.setTextSize(2);
}

// Display Bluetooth connection screen
void displayBluetoothConnectionScreen(void) {

  // Clear screen area, draw outline and title
  clearListboxArea();

  // Display credit sequence
  lcd.drawCenteredText(calcLineOffset(1), "- Waiting For -");
  lcd.drawCenteredText(calcLineOffset(2), "Bluetooth");
  lcd.drawCenteredText(calcLineOffset(3), "Connection");
}

// Display welcome screen
void displayWelcomeScreen(void) {

  char buffer[20];
  sprintf(buffer, "Version: %d.%d", MAJOR_VERSION, MINOR_VERSION);

  // Clear screen area, draw outline and title
  clearListboxArea();

  // Display credit sequence
  lcd.drawCenteredText(calcLineOffset(0), "ESP32");
  lcd.drawCenteredText(calcLineOffset(1), "CYD");
  lcd.drawCenteredText(calcLineOffset(2), "Bluetooth");
  lcd.drawCenteredText(calcLineOffset(3), "Music Player");
  lcd.drawCenteredText(calcLineOffset(4), buffer);
  lcd.drawCenteredText(calcLineOffset(5), "- Written by -");
  lcd.drawCenteredText(calcLineOffset(6), "Craig A. Lindley");
  delay(4000);
}

void displayWiFiScreen() {

  // Clear screen area
  clearListboxArea();

  // Draw string
  lcd.drawCenteredText(calcLineOffset(0), "WiFi Connecting");
}

void displayUploadScreen() {

  // Clear screen area
  clearListboxArea();

  // Draw title
  lcd.drawCenteredText(calcLineOffset(0), "Preparing For");
  lcd.drawCenteredText(calcLineOffset(1), "Remote Access");

  // Show IP address for FTP
  lcd.drawCenteredText(calcLineOffset(2), "IP Address");
  lcd.drawCenteredText(calcLineOffset(3),
                       ftpUploader.getIPAddressString().c_str());
}

// Now playing screen
void displaySongNowPlayingScreen(const char *songName) {

  // Remove the mp3 file extension
  String str(songName);

  int indx = str.lastIndexOf('.');
  if (indx != -1) {
    str = str.substring(0, indx);
  }

  // Clear screen area
  clearListboxArea();

  lcd.drawCenteredText(calcLineOffset(2), "- Now Playing -");
  lcd.setTextColor(SCREEN_TEXT_COLOR, ILI9341_BLUE);
  lcd.drawCenteredText(calcLineOffset(4), str.c_str());
  lcd.setTextColor(SCREEN_TEXT_COLOR, SCREEN_COLOR);
}

// Action screen
void displayActionScreen(boolean _looping) {

  // Clear screen area
  clearListboxArea();

  lcd.setTextColor(SCREEN_TITLE_COLOR, SCREEN_COLOR);
  lcd.drawCenteredText(calcLineOffset(0), "- Actions -");

  lcd.setTextColor(SCREEN_TEXT_COLOR, SCREEN_COLOR);
  lcd.drawCenteredText(calcLineOffset(1), "- is volume down");
  lcd.drawCenteredText(calcLineOffset(2), "+ is volume up");

  if (_looping) {
    lcd.setTextColor(ILI9341_YELLOW, SCREEN_COLOR);
  }
  lcd.drawCenteredText(calcLineOffset(3), "Back is loop off/on");

  lcd.setTextColor(SCREEN_TEXT_COLOR, SCREEN_COLOR);
  lcd.drawCenteredText(calcLineOffset(4), "Sel is done");
}

// Paint the listbox on the screen
void paintListBox(int b) {

  // Clear screen area
  clearListboxArea();

  lcd.setTextColor(SCREEN_TITLE_COLOR, SCREEN_COLOR);

  // Draw the listbox title
  lcd.drawCenteredText(LISTBOX_CONTENT_START, listBox->getTitle());

  // Moves strings over to avoid rect
  const byte xOffset = 3;

  int fontHeight = lcd.getTextHeight("A") + 2;
  int yOffset = 33;

  // Extract listbox data from argument
  int selectIndex = (b >> 16) & 0xFF;
  int windowIndex = (b >> 8) & 0xFF;
  int numberOfEntries = b & 0xFF;

  // Serial.printf("B: %d, SI: %d, WI: %d, NE: %d\n",
  //              b, selectIndex, windowIndex, numberOfEntries);

  // Fetch the centering flag
  boolean centerFlag = listBox->getCenterFlag();

  for (int i = 0; i < numberOfEntries; i++) {
    char *line = listBox->getEntry(i + windowIndex, true);

    // If we are dealing with filenames, strip extension
    String str = String(line);
    int indx = str.lastIndexOf('.');
    if (indx != -1) {
      str = str.substring(0, indx);
    }

    if ((i + windowIndex) == selectIndex) {
      // This is the selected item. Change its colors
      lcd.setTextColor(SCREEN_COLOR, SCREEN_TEXT_COLOR);
      if (!centerFlag) {
        lcd.drawText(xOffset, yOffset, str.c_str());
      } else {
        lcd.drawCenteredText(yOffset, str.c_str());
      }

    } else {
      // Non selected item
      lcd.setTextColor(SCREEN_TEXT_COLOR, SCREEN_COLOR);
      if (!centerFlag) {
        // This is a non-selected item
        lcd.drawText(xOffset, yOffset, str.c_str());
      } else {
        lcd.drawCenteredText(yOffset, str.c_str());
      }
    }
    // Calculate y offset for next line of display
    yOffset += fontHeight;
  }
  lcd.setTextColor(SCREEN_TEXT_COLOR, SCREEN_COLOR);
}

// Called anytime a button is clicked to stop backlight from turning off
// or to turn it back on
void updateTimeOut() {
  displayTimeout = millis() + DISPLAY_TIMEOUT_MS;
  lcd.backlight(true);
}

// Pick a shuffled song and place it in songPath
void pickShuffledSong() {

  // Root directory path
  // Root directory contains the artists
  strcpy(songPath, "/");

  // Pick a random artist
  int artistIndex = random(artists.size());
  strcat(songPath, artists.at(artistIndex).c_str());

  if (!populateArtistAlbumsVector(songPath)) {
    Serial.println("Artist Read Failed");
    while (true) {};
  }

  // Pick a random album
  int albumIndex = random(albums.size());
  strcat(songPath, "/");
  strcat(songPath, albums.at(albumIndex).c_str());

  if (!populateArtistAlbumSongsVector(songPath)) {
    Serial.println("Song Read Failed");
    while (true) {};
  }

  // Pick a random song
  int songIndex = random(songs.size());
  strcat(songPath, "/");
  strcat(songPath, songs.at(songIndex).c_str());
}

/****************************************************************/
/***                        Program Setup                     ***/
/****************************************************************/

void setup() {

  Serial.begin(115200);
  Serial.println("\n\nStarting Up");

  // Initialize the random number generator
  randomSeed(analogRead(34));

  // Initialize control variables
  playing = false;
  looping = false;
  skipInput = false;
  uploading = false;

  // Turn off the wifi to possibly save battery life
  WiFi.mode(WIFI_OFF);

  // Do GPIO initialization here
  // Pull all chip selects high to avoid SPI contention
  // on startup

  // LCD ILI9341 controller chip select
  pinMode(LCD_CS, OUTPUT);
  digitalWrite(LCD_CS, HIGH);

  // Touch controller chip select
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);

  // SD card chip select
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  // Initialize HSPI interface for LCD
  h_SPI.begin(LCD_SCLK, LCD_MISO, LCD_MOSI);

  // Initialize LCD ILI9341 display controller
  lcd.begin(&h_SPI);
  lcd.setRotation(SCREEN_ROTATION);
  lcd.setTextSize(2);
  lcd.clearScreen();
  lcd.setTextColor(SCREEN_TEXT_COLOR, SCREEN_COLOR);

  // Initialize the SD.
  if (!sd.begin(SD_CONFIG)) {
    sd.initErrorHalt(&Serial);
  }

  // Initialize touch screen controller
  touch.begin();


  // Display welcome screen while artists are being loaded
  displayWelcomeScreen();

  // Populate operation data source
  operations.push_back(std::string("Bluetooth"));
  operations.push_back(std::string("Sequential Play"));
  operations.push_back(std::string("Random Play"));
  operations.push_back(std::string("Shuffle"));
  operations.push_back(std::string("Remote Access"));

  // Read in all available artists
  if (!populateArtistsVector()) {
    Serial.println("Artist Read Failed");
    while (true) {};
  }

  // Instantiate the list box
  listBox = new ListBox(LISTBOX_LINES, LISTBOX_CHARS, paintListBox);
}

/****************************************************************/
/***         Program Loop Finite State Machine (FSM)          ***/
/****************************************************************/

void loop() {

  // Yield to OS tasks to help prevent drop outs
  yield();

  // Update button state
  bm.update();

  if (uploading) {
    // Feed the FTP server
    ftpUploader.handleFTP();
  }

  if (playing) {
    // Feed the BT driver
    songManager.loop();
  }

  // Runs the FSM
  switch (state) {

    // Start States
    case INITIAL:
      {
        // Turn off the wifi to possibly save battery life
        WiFi.mode(WIFI_OFF);

        // Initialize the display timeout
        displayTimeout = millis() + DISPLAY_TIMEOUT_MS;

        skipInput = false;

        // Next state
        state = OP_POPULATE_LB;
      }
      break;

    case OP_POPULATE_LB:
      {
        // Display the on screen buttons
        bm.drawButtons();

        listBox->setDataSource(OPERATION_DS);

        // Initialize list box
        listBox->clear();
        listBox->setCenterFlag(true);
        listBox->setTitle("- Operations -");

        // Paint list box
        listBox->doRepaint();

        // Next state
        state = OP_BUTTON_CHECK;
      }
      break;

    case OP_BUTTON_CHECK:
      {
        // Poll the buttons
        enum BUTTON_STATE result = bm.pollButtons();
        if (result != BS_NONE) {
          updateTimeOut();
        }

        if (result == BS_MINUS) {
          listBox->selectionUp(true);
        }

        else if (result == BS_SELECT) {
          // An action has been selected
          // Save current listbox state
          listBox->push();

          // Load artist data
          listBox->setDataSource(ARTIST_DS);

          // Next state
          state = OP_DISPATCH;
        }

        else if (result == BS_PLUS) {
          listBox->selectionDown(true);
        }

        else if (result == BS_BACK) {
          // Nothing to do here
        }
      }
      break;

    case OP_DISPATCH:
      {
        // Get selection index
        switch (listBox->getSelectionIndex()) {
          case 0:
            // Bluetooth selected
            // Next state
            state = BT_START;
            break;
          case 1:
            // Sequential play selected
            playMode = SEQUENTIAL;
            // Next state
            state = AR_POPULATE_LB;
            break;
          case 2:
            // Random play selected
            playMode = RANDOM;
            // Next state
            state = AR_POPULATE_LB;
            break;
          case 3:
            // Shuffle play selected
            // Next state
            state = SH_PICKANDPLAY;
            break;
          case 4:
            // Remote access selected
            // Next state
            state = RA_WIFI_CONNECT;
            break;
        }
      }
      break;

    case BT_START:
      {
        // Display BT connection screen
        displayBluetoothConnectionScreen();

        // Start the Song Manager which does the Bluetooth connection
        // Give it SdFat instance
        songManager.begin(&sd);

        // Next state
        state = BT_CONNECT_WAIT;
      }
      break;

    case BT_CONNECT_WAIT:
      {
        // Wait for BT connection
        while (!songManager.btConnected()) {
          delay(50);
        }

        // Pop previous menu
        listBox->pop();

        // Advance to next selection
        listBox->selectionDown(true);

        // Display the on screen buttons
        bm.drawButtons();

        // Next state
        state = OP_BUTTON_CHECK;
      }
      break;

    case AR_POPULATE_LB:
      {
        // Root directory path
        strcpy(songPath, "/");

        listBox->clear();
        listBox->setTitle("- Artists -");
        listBox->setCenterFlag(true);
        listBox->setDataSource(ARTIST_DS);

        // Paint list box
        listBox->doRepaint();

        // Next state
        state = AR_BUTTON_CHECK;
      }
      break;

    case AR_BUTTON_CHECK:
      {
        // Determine how many artists there are
        int count = listBox->getListBoxCount();
        int quarterCount = count / 4;
        int halfCount = count / 2;

        // Poll the buttons
        enum BUTTON_STATE result = bm.pollButtons();
        if (result != BS_NONE) {
          updateTimeOut();
        }

        if (result == BS_MINUS) {
          listBox->selectionUp(true);
        }

        else if (result == BS_MINUSP) {
          for (int i = 0; i < quarterCount; i++) {
            listBox->selectionUp(false);
          }
          listBox->doRepaint();
        }

        else if (result == BS_MINUSPP) {
          for (int i = 0; i < halfCount; i++) {
            listBox->selectionUp(false);
          }
          listBox->doRepaint();
        }

        else if (result == BS_PLUS) {
          listBox->selectionDown(true);
        }

        else if (result == BS_PLUSP) {
          for (int i = 0; i < quarterCount; i++) {
            listBox->selectionDown(false);
          }
          listBox->doRepaint();
        }

        else if (result == BS_PLUSPP) {
          for (int i = 0; i < halfCount; i++) {
            listBox->selectionDown(false);
          }
          listBox->doRepaint();
        }

        else if (result == BS_BACK) {
          // Null out song path
          *songPath = '\0';

          // Back to operation selection
          listBox->pop();

          // Next state
          state = OP_BUTTON_CHECK;
        }

        else if (result == BS_SELECT) {
          // An artist has been selected so save list box state
          listBox->push();

          // Next state
          state = AL_POPULATE_LB;
        }
      }
      break;

    case AL_POPULATE_LB:
      {
        // Add artist to song path
        strcat(songPath, listBox->getSelection());
        Serial.printf("SP: %s\n", songPath);

        if (!populateArtistAlbumsVector(songPath)) {
          Serial.println("Album Read Failed");
          while (true) {};
        }

        listBox->setDataSource(ALBUM_DS);

        listBox->clear();
        listBox->setTitle("- Albums/CDs -");
        listBox->setCenterFlag(true);

        // Paint list box
        listBox->doRepaint();

        // Next state
        state = AL_BUTTON_CHECK;
      }
      break;

    case AL_BUTTON_CHECK:
      {
        // Determine how many albums there are
        int count = listBox->getListBoxCount();
        int quarterCount = count / 4;
        int halfCount = count / 2;

        // Poll the buttons
        enum BUTTON_STATE result = bm.pollButtons();
        if (result != BS_NONE) {
          updateTimeOut();
        }

        if (result == BS_MINUS) {
          listBox->selectionUp(true);
        }

        else if (result == BS_MINUSP) {
          for (int i = 0; i < quarterCount; i++) {
            listBox->selectionUp(false);
          }
          listBox->doRepaint();
        }

        else if (result == BS_MINUSPP) {
          for (int i = 0; i < halfCount; i++) {
            listBox->selectionUp(false);
          }
          listBox->doRepaint();
        }

        else if (result == BS_PLUS) {
          listBox->selectionDown(true);
        }

        else if (result == BS_PLUSP) {
          for (int i = 0; i < quarterCount; i++) {
            listBox->selectionDown(false);
          }
          listBox->doRepaint();
        }

        else if (result == BS_PLUSPP) {
          for (int i = 0; i < halfCount; i++) {
            listBox->selectionDown(false);
          }
          listBox->doRepaint();
        }

        else if (result == BS_BACK) {
          // Root directory path
          strcpy(songPath, "/");

          // Back to artist selection
          listBox->pop();

          // Next state
          state = AR_BUTTON_CHECK;
        }

        else if (result == BS_SELECT) {
          // An album has been selected so save list box state
          listBox->push();

          // Next state
          state = SG_POPULATE_LB;
        }
      }
      break;

    case SG_POPULATE_LB:
      {
        // Add album to song path
        strcat(songPath, "/");
        strcat(songPath, listBox->getSelection());

        if (!populateArtistAlbumSongsVector(songPath)) {
          Serial.println("Song Read Failed");
          while (true) {};
        }
        listBox->setDataSource(SONG_DS);

        listBox->clear();
        listBox->setTitle("- Songs -");
        listBox->setCenterFlag(true);

        // Paint list box
        listBox->doRepaint();

        // Next state
        state = SG_BUTTON_CHECK;
      }
      break;

    case SG_BUTTON_CHECK:
      {
        // Poll the buttons
        enum BUTTON_STATE result = bm.pollButtons();
        if (result != BS_NONE) {
          updateTimeOut();
        }

        if (result == BS_MINUS) {
          listBox->selectionUp(true);
        }

        else if (result == BS_PLUS) {
          listBox->selectionDown(true);
        }

        else if (result == BS_BACK) {
          // Find last forward slash in file path
          char *lastSlash = strrchr(songPath, 0x2F);

          // Terminate song path there
          *lastSlash = '\0';

          // Back to album selection
          listBox->pop();

          // Next state
          state = AL_BUTTON_CHECK;
        }

        else if (result == BS_SELECT) {
          // An song has been selected so save list box state
          listBox->push();

          // Next state
          state = SG_PLAY;
        }
      }
      break;

    case SG_PLAY:
      {
        // Stop any song playing
        playing = false;
        songManager.stopSong();

        // Add song filename to song path
        strcat(songPath, "/");
        strcat(songPath, listBox->getSelection());

        Serial.printf("File to play: %s\n", songPath);

        // Display the song to play
        displaySongNowPlayingScreen(listBox->getSelection());

        // Turn display back on if off for song change
        updateTimeOut();

        // Play the song
        songManager.playSong(songPath);

        playing = true;

        // Next state
        state = SG_SONGSTATUS_CHECK;
      }
      break;

    case SG_SONGSTATUS_CHECK:
      {
        // Has the song ended ?
        if (!songManager.isActive()) {
          // Song has ended

          // Stop any song playing
          playing = false;
          songManager.stopSong();

          // Are we looping on this song ?
          if (looping) {
            // Looping
          } else {
            // Not looping
            if (playMode == SEQUENTIAL) {
              listBox->selectionDown(false);
            } else {
              listBox->selectRandomEntry(false);
            }
            listBox->updatePush();
          }

          // Next state
          state = SG_PATH_RESET;
          break;
        }
        // Song is playing so read buttons
        enum BUTTON_STATE result = bm.pollButtons();

        // If skipInput is true we are trying to consume
        // the first button press because it should just
        // turn the backlight back on

        if (skipInput) {
          if (result != 0) {
            skipInput = false;
            updateTimeOut();

            // Stay in this state
            break;
          }
        }
        if (result != BS_NONE) {
          updateTimeOut();
        }

        if (result == BS_MINUS) {
          // Stop any song playing
          playing = false;
          songManager.stopSong();

          listBox->selectionUp(false);
          listBox->updatePush();
          
          // Next state
          state = SG_PATH_RESET;
        }

        else if (result == BS_PLUS) {
          // Stop any song playing
          playing = false;
          songManager.stopSong();

          listBox->selectionDown(false);
          listBox->updatePush();
          
          // Next state
          state = SG_PATH_RESET;
        }

        else if (result == BS_BACK) {
          songManager.stopSong();
          playing = false;

          // Back to song selection
          listBox->pop();

          // Remove previous song from song path
          char *lastSlash = strrchr(songPath, 0x2F);
          *lastSlash = '\0';

          // Next state
          state = SG_BUTTON_CHECK;
        }

        else if ((result == BS_SELECT) || (result == BS_TOUCHED)) {
          // Select button during song playback brings up actions screen
          // Pause the music
          songManager.stopSong();
          playing = false;

          // Next state
          state = AC_DISPLAY;
        }

        else if (millis() > displayTimeout) {
          // If skipInput is true
          // Ignore the button press that turns the display backlight on
          skipInput = true;
          lcd.backlight(LOW);
        }
      }
      break;

    case SG_PATH_RESET:
      {
        // Remove previous song from song path
        char *lastSlash = strrchr(songPath, 0x2F);
        *lastSlash = '\0';

        // Next state
        state = SG_PLAY;
      }
      break;

    case AC_DISPLAY:
      {
        // Display the action screen
        displayActionScreen(looping);

        // Next state
        state = AC_BUTTON_CHECK;
      }
      break;

    case AC_BUTTON_CHECK:
      {
        // Poll the buttons
        enum BUTTON_STATE result = bm.pollButtons();
        if (result != BS_NONE) {
          updateTimeOut();
        }

        if (result == BS_MINUS) {
          songManager.volumeDown();
        }

        else if (result == BS_PLUS) {
          songManager.volumeUp();
        }

        else if (result == BS_BACK) {
          looping = !looping;

          // Next state
          state = AC_DISPLAY;
        }

        else if (result == BS_SELECT) {
          // Turn music back on
          songManager.resume();
          playing = true;

          // Display song now playing
          displaySongNowPlayingScreen(listBox->getSelection());

          // Next state
          state = SG_SONGSTATUS_CHECK;
        }
      }
      break;

    case SH_PICKANDPLAY:
      {
        // Stop any song playing
        playing = false;
        songManager.stopSong();

        // Pick a shuffled song
        pickShuffledSong();

        // Extract the song's name from the song's path
        String sps = String(songPath);

        String fileName = sps.substring(sps.lastIndexOf('/') + 1,
                                        sps.length());
        // Display song now playing
        displaySongNowPlayingScreen(fileName.c_str());

        // Play the song
        songManager.playSong(songPath);

        playing = true;

        // Next state
        state = SH_BUTTONSTATUS_CHECK;
      }
      break;

    case SH_BUTTONSTATUS_CHECK:
      {
        // Has the song ended ?
        if (!songManager.isActive()) {
          // Song has ended
          // Pick a new song to play
          state = SH_PICKANDPLAY;
          break;
        }

        // Song is playing so read buttons
        enum BUTTON_STATE result = bm.pollButtons();

        // If skipInput is true we are trying to consume
        // the first button press because it should just
        // turn the backlight back on

        if (skipInput) {
          if (result != 0) {
            skipInput = false;
            updateTimeOut();
            // Stay in this state
            break;
          }
        }
        if (result != BS_NONE) {
          updateTimeOut();
        }

        if (result == BS_MINUS) {
          // Next state
          state = SH_PICKANDPLAY;
        }

        else if (result == BS_PLUS) {
          // Next state
          state = SH_PICKANDPLAY;
        }

        else if (result == BS_BACK) {
          songManager.stopSong();
          playing = false;

          // Back to song selection
          listBox->pop();

          // Next state
          state = OP_BUTTON_CHECK;
        }

        else if (result == BS_SELECT) {
          // Select button doesn't do anything
        }

        else if (millis() > displayTimeout) {
          // If skipInput is true
          // Ignore the button press that turns the display backlight on
          skipInput = true;
          lcd.backlight(LOW);
        }
      }
      break;

    case RA_WIFI_CONNECT:
      {
        // Display the WiFi screen
        displayWiFiScreen();

        // Attempt WiFi connection
        if (!ftpUploader.begin(&sd)) {

          lcd.setTextColor(ILI9341_RED, SCREEN_COLOR);

          for (int i = 0; i < 7; i++) {
            lcd.drawCenteredText(calcLineOffset(2), "Connect Failed");
            delay(500);
            lcd.clearScreen();
            delay(500);
          }
          // Reboot the ESP32
          ESP.restart();
        }

        // Next state
        state = RA_DISPLAY;
      }
      break;

    case RA_DISPLAY:
      {
        displayUploadScreen();

        // Indicating we are uploading
        uploading = true;

        // Next state
        state = RA_BUTTON_CHECK;
      }
      break;

    case RA_BUTTON_CHECK:
      {
        // Poll the switches. Any switch press cancels FTP
        enum BUTTON_STATE result = bm.pollButtons();
        if (result != BS_NONE) {

          // Indicate not uploading
          uploading = false;

          // Next state is complete restart
          state = INITIAL;
        }
      }
      break;
  };
}
