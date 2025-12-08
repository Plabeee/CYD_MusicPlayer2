/*
   This class controls the operation of the FTPServer for accessing music files

   Concept, design and implementation by: Craig A. Lindley
   Last Update: 11/06/2025
*/

#ifndef FTPUPLOADER_H
#define FTPUPLOADER_H

#include "FTPServer.h"

class FTPUploader {

  public:

    // Class Constructor
    FTPUploader(void) {
      connected = false;
    }

    // Connect to WiFi and initialize FTP server
    boolean begin(SdFat32 *ptrSd) {

      // Establish WiFi connection
      if (! canConnect()) {
        return false;
      }

      // Initialize the FTP server with username and password for connection
      ftpServer.begin(FTP_USER, FTP_PSWD, ptrSd);

      return true;
    }

    boolean isConnected(void) {
      return connected;
    }

    String getIPAddressString(void) {
      if (connected) {
        return String(WiFi.localIP().toString());
      } else  {
        return String("No connection");
      }
    }

    void handleFTP(void) {
      if (connected) {
        ftpServer.handleFTP();
      }
    }

  protected:

    // Attempt connection to WiFi
    // Returns true if successful
    boolean canConnect(void) {
      
      WiFi.mode(WIFI_STA);
      delay(100);
      WiFi.begin(WIFI_NAME, WIFI_PSWD);
      delay(100);

      // Number of connection attempts before failure
      int attempts = WIFI_ATTEMPTS;

      while ((WiFi.status() != WL_CONNECTED) && attempts--) {
        delay(500);
      }

      // Did we connect ?
      if (attempts <= 0) {
        Serial.printf("Could not connect to WiFi network\n");
        connected = false;
        // No connection so return indication
        return false;
      } else  {
        Serial.printf("WiFi connected\n");
        connected = true;
        return true;
      }
    }


    boolean connected;

    // Declare FTP server instance
    FTPServer ftpServer;
};

#endif
