/*
   ListBox's non-GUI Component for ESP32 Music Player

   Concept, design and implementation by: Craig A. Lindley
   Last Update: 11/06/2025
*/

#ifndef LISTBOX_H
#define LISTBOX_H

#include <string>
#include <vector>

// Storage for operation data
std::vector<std::string> operations;

// Storage for the music data
std::vector<std::string> artists;
std::vector<std::string> albums;
std::vector<std::string> songs;

// Data source identifer for List Box
enum DATA_SOURCE {OPERATION_DS, ARTIST_DS, ALBUM_DS, SONG_DS};

#define MAX_LINE_LENGTH    40
#define MAX_TITLE_LENGTH   18
#define NUM_OF_SAVED_STATES 4

// Macros for handling random play flags
#define bitRead64(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet64(value, bit) ((value) |= (1ULL << (bit)))

// How many time we will attempt to generate a random select index
// before clearing the flags and trying again
#define MAX_SPINS 45

// Storage of listbox state
typedef struct {
  char title[MAX_TITLE_LENGTH + 1];
  int savedSelectIndex;
  int savedWindowIndex;
  boolean savedCenterFlag;
  enum DATA_SOURCE savedDataSource;
} SAVEDSTATE;

SAVEDSTATE stack[NUM_OF_SAVED_STATES];
int stackIndex;

char buffer[MAX_LINE_LENGTH + 1];
char title[MAX_TITLE_LENGTH + 1];

// Callback is a pointer to a application function that takes a single int argument and
// repaints the GIU when called.
typedef void (*callback)(int);

class ListBox {

  public:

    // Class constructor
    // _numberOfLines is the number of lines in the listbox window
    //     that can be displayed on the display.
    // _maxCharCount is the max number of chars that can be displayed
    //    per line in the listbox window in the currently selected font.
    // _repaint is a function called whenever the listbox needs updating
    ListBox(int _numberOfLines, int _maxCharCount, callback _repaint) {

      // Save incoming
      numberOfLines = _numberOfLines;
      maxCharCount = _maxCharCount;
      repaint = _repaint;

      // Clear stack storage
      memset(stack, 0, sizeof(stack));
      stackIndex = 0;

      dataSourceID = OPERATION_DS;
      dataSourceCount = operations.size();

      // Initialize the string storage
      clear();
    }

    // Clear listbox of all data
    // NOTE: stack storage not cleared
    void clear() {
      // Initialize string storage
      selectIndex  = 0;
      windowIndex  = 0;
      centerFlag = false;

      // Clear flags variable
      flags = 0;
      spins = 0;

      // Clear title of listbox
      memset(title, 0, sizeof(title));
    }

    // Indicate where the listbox should get its backing data
    void setDataSource(enum DATA_SOURCE ds) {
      dataSourceID = ds;

      switch (dataSourceID) {
        case OPERATION_DS:
          dataSourceCount = operations.size();
          break;
        case ARTIST_DS:
          dataSourceCount = artists.size();
          break;
        case ALBUM_DS:
          dataSourceCount = albums.size();
          break;
        case SONG_DS:
          dataSourceCount = songs.size();
          break;
      }
      // Serial.printf("C: %d\n", dataSourceCount);
    }

    // Get a count of the listbox entries
    int getListBoxCount() {
      return dataSourceCount;
    }

    // Push the current listbox context onto the stack for later restoration
    void push() {

      SAVEDSTATE *pState = &stack[stackIndex++];

      pState->savedSelectIndex = selectIndex;
      pState->savedWindowIndex = windowIndex;
      pState->savedCenterFlag  = centerFlag;
      pState->savedDataSource  = dataSourceID;

      strcpy(pState->title, title);
    }

    // Update the current listbox context for later restoration
    void updatePush() {

      // Get pointer to latest stack entry
      SAVEDSTATE *pState = &stack[stackIndex - 1];

      pState->savedSelectIndex = selectIndex;
      pState->savedWindowIndex = windowIndex;
    }

    // Restore the current listbox context from the stack
    void pop() {

      SAVEDSTATE state = stack[--stackIndex];

      selectIndex  = state.savedSelectIndex;
      windowIndex  = state.savedWindowIndex;
      centerFlag   = state.savedCenterFlag;
      dataSourceID = state.savedDataSource;

      setDataSource(dataSourceID);

      strcpy(title, state.title);

      // Clear flags variable
      flags = 0;
      spins = 0;

      // After data is restored, do a repaint
      doRepaint();
    }

    // Set the listbox title
    void setTitle(const char *str) {
      memset(title, 0, sizeof(title));
      strncpy(title, str, MAX_TITLE_LENGTH);
    }

    // Get the listbox title
    char *getTitle() {
      return title;
    }

    // Set the centering flag
    void setCenterFlag(boolean flag) {
      centerFlag = flag;
    }

    boolean getCenterFlag(void) {
      return centerFlag;
    }

    void doRepaint() {
      repaint(selectIndex << 16 | windowIndex << 8 | min(dataSourceCount, numberOfLines));
    }

    void selectionUp(boolean repaint) {
      // Is there an item above ?
      if (selectIndex > 0) {
        // Yes so move selectIndex up
        // Will moving up keep selection in window?
        if (selectIndex > windowIndex) {
          selectIndex--;
          // No window has to move
        } else if (windowIndex > 0) {
          windowIndex--;
          selectIndex--;
        }
      } else  {
        // No, must reposition to end of list
        selectIndex = dataSourceCount - 1;
        windowIndex = dataSourceCount - numberOfLines;
        if (windowIndex < 0) {
          windowIndex = 0;
        }
      }
      if (repaint) {
        doRepaint();
      }
    }

    void selectionDown(boolean repaint) {

      // If not at end of storage already
      if (selectIndex < dataSourceCount - 1) {

        // Will moving down keep selection in window?
        if ((selectIndex >= windowIndex) && (selectIndex < (windowIndex + numberOfLines - 1))) {
          selectIndex++;

          // No window has to move
        } else if ((windowIndex + numberOfLines) < dataSourceCount) {
          windowIndex++;
          selectIndex++;
        }
      } else  {
        selectIndex = 0;
        windowIndex = 0;
      }
      if (repaint) {
        doRepaint();
      }
    }

    // Select a random entry from the list box
    // The code below tries to play all songs in a directory randomly
    // before repeating any.
    void selectRandomEntry(boolean repaint) {

      // Log the song just played
      bitSet64(flags, selectIndex);

      boolean done = false;
      while (! done) {
        // Random count of steps to scroll down in list box
        int steps = random(dataSourceCount);
        for (int i = 0; i < steps; i++) {
          selectionDown(false);
        }
        // Check to see if the selected song index has been selected before
        if (bitRead64(flags, selectIndex) == 0) {
          // No it hasn't so indicate it has now
          bitSet64(flags, selectIndex);
          // Reset number of spins because new song @ selectIndex has been selected
          spins = 0;
          done = true;
        } else {
          // The selected song index has been selected before, so try again
          // If we have exceeded max attempts then reset flags and start random
          // sequence over.
          if (++spins > MAX_SPINS) {
            flags = 0;
            spins = 0;
          }
        }
      }
      if (repaint) {
        doRepaint();
      }
    }

    // Return the current selection. i.e. the item highlighted in the listbox
    // in its full form.
    const char *getSelection() {

      switch (dataSourceID) {
        case OPERATION_DS:
          return operations.at(selectIndex).c_str();
        case ARTIST_DS:
          return artists.at(selectIndex).c_str();
        case ALBUM_DS:
          return albums.at(selectIndex).c_str();
        case SONG_DS:
          return songs.at(selectIndex).c_str();
      }
      return "";
    }

    // Return the index of the current selection. i.e. the item highlighted in the listbox
    int getSelectionIndex() {
      return selectIndex;
    }

    // Retrieve a pointer to the string in the listbox with specified index
    char *getEntry(int index, boolean clip) {

      char *str;
      
      switch (dataSourceID) {
        case OPERATION_DS:
          str = (char *) operations.at(index).c_str();
          break;
        case ARTIST_DS:
          str = (char *) artists.at(index).c_str();
          break;
        case ALBUM_DS:
          str = (char *) albums.at(index).c_str();
          break;
        case SONG_DS:
          str = (char *) songs.at(index).c_str();
          break;
      }

      if (clip) {
        // Clip string for display
        return clipString(str);
      } else  {
        // Return full string
        return str;
      }
    }

  private:

    // Flags to prevent song repeats when playing songs randomly
    // Maximum of 64 entries supported. See macros at top of file
    uint64_t flags;
    int spins;

    enum DATA_SOURCE dataSourceID;
    int dataSourceCount;

    int numberOfLines;
    int maxCharCount;
    callback repaint;

    int selectIndex;
    int windowIndex;
    boolean centerFlag;

    // Clip a string to specified length
    char *clipString(char *str) {
      memset(buffer, 0, sizeof(buffer));
      strncpy(buffer, str, maxCharCount);
      return buffer;
    }
};

#endif
