/*
   FTP Server for ESP32 with attached SD Card

   based on FTP Serveur for Arduino Due and Ethernet shield by
   Jean-Michel Gallego (https://github.com/gallegojm/Arduino-Ftp-Server/tree/master/FtpServer)

   modified to work with esp8266 and ESP32 and SPIFFS by
   David Paiva (https://github.com/nailbuster/esp8266FTPServer)

   modified to work with ESP32 with attached SD card by Craig A. Lindley

   NOTE: tested only with FileZilla

   Last Update: 11/06/2025
*/

// Uncomment to print debugging info to console attached to ESP32
// #define FTP_DEBUG

#ifndef FTPSERVER_H
#define FTPSERVER_H

#include <WiFi.h>
#include <WiFiClient.h>

#define FTP_SERVER_VERSION "FTP-2018-08-10"

#define FTP_CTRL_PORT 21          // Command port on wich server is listening
#define FTP_DATA_PORT_PASV 50009  // Data port in passive mode

#define FTP_TIME_OUT 15        // Disconnect client after 15 minutes of inactivity
#define FTP_CMD_SIZE 255 + 8   // max size of a command
#define FTP_CWD_SIZE 255 + 8   // max size of a directory name
#define FTP_FIL_SIZE 255       // max size of a file name
#define FTP_BUF_SIZE 2 * 1460  // size of file buffer for read/write

// Instantiate the FTP control and data servers
WiFiServer controlServer(FTP_CTRL_PORT);
WiFiServer dataServer(FTP_DATA_PORT_PASV);

char NAME_BUFFER[128];

class FTPServer {

public:

  void begin(String uname, String pword, SdFat32 *ptrSd) {

    // Tells the ftp server to begin listening for incoming connection
    _FTP_USER = uname;
    _FTP_PASS = pword;

    _ptrSd = ptrSd;

    controlServer.begin();
    delay(10);
    dataServer.begin();
    delay(10);
    millisTimeOut = (uint32_t)FTP_TIME_OUT * 60 * 1000;
    millisDelay = 0;
    cmdStatus = 0;
    initVariables();
  }

  void handleFTP() {

    if ((int32_t)(millisDelay - millis()) > 0) {
      return;
    }

    if (controlServer.hasClient()) {
      client.stop();
      client = controlServer.available();
    }

    if (cmdStatus == 0) {
      if (client.connected()) {
        disconnectClient();
      }
      cmdStatus = 1;
    } else if (cmdStatus == 1) {  // Ftp server waiting for connection
      abortTransfer();
      initVariables();
#ifdef FTP_DEBUG
      Serial.println("Ftp server waiting for connection on port " + String(FTP_CTRL_PORT));
#endif
      cmdStatus = 2;
    } else if (cmdStatus == 2) {  // Ftp server idle
      if (client.connected()) {   // A client connected
        clientConnected();
        millisEndConnection = millis() + 10 * 1000;  // wait client id during 10 s.
        cmdStatus = 3;
      }
    } else if (readChar() > 0) {  // got response
      if (cmdStatus == 3) {       // Ftp server waiting for user identity
        if (userIdentity()) {
          cmdStatus = 4;
        } else {
          cmdStatus = 0;
        }
      } else if (cmdStatus == 4) {  // Ftp server waiting for user registration
        if (userPassword()) {
          cmdStatus = 5;
          millisEndConnection = millis() + millisTimeOut;
        } else {
          cmdStatus = 0;
        }
      } else if (cmdStatus == 5) {  // Ftp server waiting for user command
        if (!processCommand()) {
          cmdStatus = 0;
        } else {
          millisEndConnection = millis() + millisTimeOut;
        }
      }
    } else if (!client.connected() || !client) {
      cmdStatus = 1;
#ifdef FTP_DEBUG
      Serial.println("client disconnected");
#endif
    }

    if (transferStatus == 1) {  // Retrieve data
      if (!doRetrieve()) {
        transferStatus = 0;
      }
    } else if (transferStatus == 2) {  // Store data
      if (!doStore()) {
        transferStatus = 0;
      }
    } else if (cmdStatus > 2 && !((int32_t)(millisEndConnection - millis()) > 0)) {
      client.println("530 Timeout");
      millisDelay = millis() + 200;  // delay of 200 ms
      cmdStatus = 0;
    }
  }

private:

  void initVariables() {

    // Default for data port
    dataPort = FTP_DATA_PORT_PASV;

    // Default Data connection is Active
    dataPassiveConn = true;

    // Set the root directory
    strcpy(cwdName, "/");

    rnfrCmd = false;
    transferStatus = 0;
  }

  void clientConnected() {

#ifdef FTP_DEBUG
    Serial.println("Client connected!");
#endif
    client.println("220--- Welcome to FTP for ESP32 ---");
    client.println("220---   By Jean-Michel Gallego/David Paiva/Craig Lindley   ---");
    client.println("220 --   Version " + String(FTP_SERVER_VERSION) + "   --");
    iCL = 0;
  }

  void disconnectClient() {

#ifdef FTP_DEBUG
    Serial.println(" Disconnecting client");
#endif
    abortTransfer();
    client.println("221 Goodbye");
    client.stop();
  }

  boolean userIdentity() {

    if (strcmp(command, "USER"))
      client.println("500 Syntax error");
    if (strcmp(parameters, _FTP_USER.c_str()))
      client.println("530 user not found");
    else {
      client.println("331 OK. Password required");
      strcpy(cwdName, "/");
      return true;
    }
    millisDelay = millis() + 100;  // delay of 100 ms
    return false;
  }

  boolean userPassword() {

    if (strcmp(command, "PASS"))
      client.println("500 Syntax error");
    else if (strcmp(parameters, _FTP_PASS.c_str()))
      client.println("530 ");
    else {
#ifdef FTP_DEBUG
      Serial.println("OK. Waiting for commands.");
#endif
      client.println("230 OK.");
      return true;
    }
    millisDelay = millis() + 100;  // delay of 100 ms
    return false;
  }

  boolean processCommand() {

    ///////////////////////////////////////
    //                                   //
    //      ACCESS CONTROL COMMANDS      //
    //                                   //
    ///////////////////////////////////////

    //
    //  CDUP - Change to Parent Directory
    //
    if (!strcmp(command, "CDUP")) {
      boolean ok = false;

      if (strlen(cwdName) > 1)  // do nothing if cwdName is root
      {
        // if cwdName ends with '/', remove it (must not append)
        if (cwdName[strlen(cwdName) - 1] == '/')
          cwdName[strlen(cwdName) - 1] = 0;
        // search last '/'
        char *pSep = strrchr(cwdName, '/');
        ok = pSep > cwdName;
        // if found, ends the string on its position
        if (ok) {
          *pSep = 0;
          ok = _ptrSd->exists(cwdName);
        }
      }
      // if an error appends, move to root
      if (!ok) {
        strcpy(cwdName, "/");
      }
      client.println("200 Ok. Current directory is " + String(cwdName));
    }

    //
    //  CWD - Change Working Directory
    //
    else if (!strcmp(command, "CWD")) {
      char path[FTP_CWD_SIZE];

      if (strcmp(parameters, ".") == 0)  // 'CWD .' is the same as PWD command
        client.println("257 \"" + String(cwdName) + "\" is your current directory");

      else if (makePath(path)) {
        if (!_ptrSd->exists(path))
          client.println("550 Can't change directory to " + String(parameters));
        else {
          strcpy(cwdName, path);
          client.println("250 Ok. Current directory is " + String(cwdName));
          ;
        }
      }
    }

    //
    //  PWD - Print Directory
    //
    else if (!strcmp(command, "PWD"))
      client.println("257 \"" + String(cwdName) + "\" is your current directory");
    //
    //  QUIT
    //
    else if (!strcmp(command, "QUIT")) {
      disconnectClient();
      return false;
    }

    ///////////////////////////////////////
    //                                   //
    //    TRANSFER PARAMETER COMMANDS    //
    //                                   //
    ///////////////////////////////////////

    //
    //  MODE - Transfer Mode
    //
    else if (!strcmp(command, "MODE")) {
      if (!strcmp(parameters, "S"))
        client.println("200 S Ok");
      // else if( ! strcmp( parameters, "B" ))
      //  client.println( "200 B Ok\r\n";
      else
        client.println("504 Only S(tream) is suported");
    }

    //
    //  PASV - Passive Connection management
    //
    else if (!strcmp(command, "PASV")) {
      if (data.connected()) data.stop();
      dataIp = client.localIP();
      dataPort = FTP_DATA_PORT_PASV;
#ifdef FTP_DEBUG
      Serial.println("Connection management set to passive");
      Serial.println("Data port set to " + String(dataPort));
#endif
      client.println("227 Entering Passive Mode (" + String(dataIp[0]) + "," + String(dataIp[1]) + "," + String(dataIp[2]) + "," + String(dataIp[3]) + "," + String(dataPort >> 8) + "," + String(dataPort & 255) + ").");
      dataPassiveConn = true;
    }

    //
    //  PORT - Data Port
    //
    else if (!strcmp(command, "PORT")) {
      if (data) data.stop();
      // get IP of data client
      dataIp[0] = atoi(parameters);
      char *p = strchr(parameters, ',');
      for (uint8_t i = 1; i < 4; i++) {
        dataIp[i] = atoi(++p);
        p = strchr(p, ',');
      }
      // get port of data client
      dataPort = 256 * atoi(++p);
      p = strchr(p, ',');
      dataPort += atoi(++p);
      if (p == NULL)
        client.println("501 Can't interpret parameters");
      else {

        client.println("200 PORT command successful");
        dataPassiveConn = false;
      }
    }

    //
    //  STRU - File Structure
    //
    else if (!strcmp(command, "STRU")) {
      if (!strcmp(parameters, "F"))
        client.println("200 F Ok");
      // else if( ! strcmp( parameters, "R" ))
      //  client.println( "200 B Ok\r\n";
      else
        client.println("504 Only F(ile) is suported");
    }

    //
    //  TYPE - Data Type
    //
    else if (!strcmp(command, "TYPE")) {
      if (!strcmp(parameters, "A"))
        client.println("200 TYPE is now ASII");
      else if (!strcmp(parameters, "I"))
        client.println("200 TYPE is now 8-bit binary");
      else
        client.println("504 Unknow TYPE");
    }

    ///////////////////////////////////////
    //                                   //
    //        FTP SERVICE COMMANDS       //
    //                                   //
    ///////////////////////////////////////

    //
    //  ABOR - Abort
    //
    else if (!strcmp(command, "ABOR")) {
      abortTransfer();
      client.println("226 Data connection closed");
    }

    //
    //  DELE - Delete a File
    //
    else if (!strcmp(command, "DELE")) {
      char path[FTP_CWD_SIZE];
      if (strlen(parameters) == 0)
        client.println("501 No file name");
      else if (makePath(path)) {
        if (!_ptrSd->exists(path))
          client.println("550 File " + String(parameters) + " not found");
        else {
          if (_ptrSd->remove(path))
            client.println("250 Deleted " + String(parameters));
          else
            client.println("450 Can't delete " + String(parameters));
        }
      }
    }

    //
    //  LIST - List
    //
    else if (!strcmp(command, "LIST")) {
      if (!dataConnect())
        client.println("425 No data connection");
      else {
        client.println("150 Accepted data connection");
        uint16_t nm = 0;
        File root = _ptrSd->open(cwdName);
        if (!root) {
          client.println("550 Can't open directory " + String(cwdName));
          // return;
        } else {
          // if(!root.isDirectory()){
          //     Serial.println("Not a directory");
          //    return;
          // }

          File file = root.openNextFile();
          while (file) {
            if (file.isDirectory()) {
              file.getName(NAME_BUFFER, sizeof(NAME_BUFFER));
              data.println("+r,s <DIR> " + String(NAME_BUFFER));
              // Serial.print("  DIR : ");
              // Serial.println(file.name());
              // if(levels){
              //  listDir(fs, file.name(), levels -1);
              // }
            } else {
              String fn, fs;
              file.getName(NAME_BUFFER, sizeof(NAME_BUFFER));
              fs = String(file.size());
              data.println("+r,s" + fs);
              data.println(",\t" + String(NAME_BUFFER));
              nm++;
            }
            file = root.openNextFile();
          }
          client.println("226 " + String(nm) + " matches total");
        }
        data.stop();
      }
    }

    //
    //  MLSD - Listing for Machine Processing (see RFC 3659)
    //
    else if (!strcmp(command, "MLSD")) {
      if (!dataConnect())
        client.println("425 No data connection MLSD");
      else {
        client.println("150 Accepted data connection");
        uint16_t nm = 0;
        File root = _ptrSd->open(cwdName);
        if (!root) {
          client.println("550 Can't open directory " + String(cwdName));
        } else {
          File file = root.openNextFile();
          while (file) {
            // Get filename then remove all references to its path
            file.getName(NAME_BUFFER, sizeof(NAME_BUFFER));
            String fn = String(NAME_BUFFER);
            int fsIndex = fn.lastIndexOf('/');
            fn = fn.substring(fsIndex + 1);
            if (file.isDirectory()) {
              data.println("Type=dir; " + fn);
            } else {
              String fs = String(file.size());
              data.println("Type=file;Size=" + fs + "; " + fn);
              nm++;
            }
            file = root.openNextFile();
          }
          client.println("226-options: -a -l");
          client.println("226 " + String(nm) + " matches total");
          data.stop();
        }
      }
    }

    //
    //  NLST - Name List
    //
    else if (!strcmp(command, "NLST")) {
      if (!dataConnect())
        client.println("425 No data connection");
      else {
        client.println("150 Accepted data connection");
        uint16_t nm = 0;

        File root = _ptrSd->open(cwdName);
        if (!root) {
          client.println("550 Can't open directory " + String(cwdName));
        } else {

          File file = root.openNextFile();
          while (file) {
            file.getName(NAME_BUFFER, sizeof(NAME_BUFFER));
            data.println(NAME_BUFFER);
            nm++;
            file = root.openNextFile();
          }
          client.println("226 " + String(nm) + " matches total");
        }
        data.stop();
      }
    }

    //
    //  NOOP
    //
    else if (!strcmp(command, "NOOP")) {
      // dataPort = 0;
      client.println("200 Zzz...");
    }

    //
    //  RETR - Retrieve
    //
    else if (!strcmp(command, "RETR")) {
      char path[FTP_CWD_SIZE];
      if (strlen(parameters) == 0)
        client.println("501 No file name");
      else if (makePath(path)) {
        file = _ptrSd->open(path, FILE_READ);
        if (!file)
          client.println("550 File " + String(parameters) + " not found");
        else if (!file)
          client.println("450 Can't open " + String(parameters));
        else if (!dataConnect())
          client.println("425 No data connection");
        else {
#ifdef FTP_DEBUG
          Serial.println("Sending " + String(parameters));
#endif
          client.println("150-Connected to port " + String(dataPort));
          client.println("150 " + String(file.size()) + " bytes to download");
          millisBeginTrans = millis();
          bytesTransfered = 0;
          transferStatus = 1;
        }
      }
    }

    //
    //  STOR - Store
    //
    else if (!strcmp(command, "STOR")) {
      char path[FTP_CWD_SIZE];
      if (strlen(parameters) == 0)
        client.println("501 No file name");
      else if (makePath(path)) {
        file = _ptrSd->open(path, FILE_WRITE);
        if (!file)
          client.println("451 Can't open/create " + String(parameters));
        else if (!dataConnect()) {
          client.println("425 No data connection");
          file.close();
        } else {
#ifdef FTP_DEBUG
          Serial.println("Receiving " + String(parameters));
#endif
          client.println("150 Connected to port " + String(dataPort));
          millisBeginTrans = millis();
          bytesTransfered = 0;
          transferStatus = 2;
        }
      }
    }

    //
    //  MKD - Make Directory
    //
    else if (!strcmp(command, "MKD")) {
      char path[FTP_CWD_SIZE];
      if (strlen(parameters) == 0)
        client.println("501 No directory name");
      else if (makePath(path)) {
        if (_ptrSd->exists(path))
          client.println("521 \"" + String(parameters) + "\" directory already exists");
        else {
#ifdef FTP_DEBUG
          Serial.println("Creating directory " + String(parameters));
#endif
          if (_ptrSd->mkdir(path))
            client.println("257 \"" + String(parameters) + "\" created");
          else
            client.println("550 Can't create \"" + String(parameters) + "\"");
        }
      }
    }

    //
    //  RMD - Remove a Directory
    //
    else if (!strcmp(command, "RMD")) {
      char path[FTP_CWD_SIZE];
      if (strlen(parameters) == 0)
        client.println("501 No directory name");
      else if (makePath(path)) {
#ifdef FTP_DEBUG
        Serial.println("Deleting " + String(path));
#endif
        if (!_ptrSd->exists(path))
          client.println("550 File " + String(parameters) + " not found");
        else if (_ptrSd->rmdir(path))
          client.println("250 \"" + String(parameters) + "\" deleted");
        else
          client.println("501 Can't delete \"" + String(parameters) + "\"");
      }
    }

    //
    //  RNFR - Rename From
    //
    else if (!strcmp(command, "RNFR")) {
      buf[0] = 0;
      if (strlen(parameters) == 0)
        client.println("501 No file name");
      else if (makePath(buf)) {
        if (!_ptrSd->exists(buf))
          client.println("550 File " + String(parameters) + " not found");
        else {
#ifdef FTP_DEBUG
          Serial.println("Renaming " + String(buf));
#endif
          client.println("350 RNFR accepted - file exists, ready for destination");
          rnfrCmd = true;
        }
      }
    }

    //
    //  RNTO - Rename To
    //
    else if (!strcmp(command, "RNTO")) {
      char path[FTP_CWD_SIZE];
      char dir[FTP_FIL_SIZE];
      if (strlen(buf) == 0 || !rnfrCmd)
        client.println("503 Need RNFR before RNTO");
      else if (strlen(parameters) == 0)
        client.println("501 No file name");
      else if (makePath(path)) {
        if (_ptrSd->exists(path))
          client.println("553 " + String(parameters) + " already exists");
        else {
#ifdef FTP_DEBUG
          Serial.println("Renaming " + String(buf) + " to " + String(path));
#endif
          if (_ptrSd->rename(buf, path))
            client.println("250 File successfully renamed or moved");
          else
            client.println("451 Rename/move failure");
        }
      }
      rnfrCmd = false;
    }

    ///////////////////////////////////////
    //                                   //
    //   EXTENSIONS COMMANDS (RFC 3659)  //
    //                                   //
    ///////////////////////////////////////

    //
    //  FEAT - New Features
    //
    else if (!strcmp(command, "FEAT")) {
      client.println("211-Extensions suported:");
      client.println(" MLSD");
      client.println("211 End.");
    }

    //
    //  MDTM - File Modification Time (see RFC 3659)
    //
    else if (!strcmp(command, "MDTM")) {
      client.println("550 Unable to retrieve time");
    }

    //
    //  SIZE - Size of the file
    //
    else if (!strcmp(command, "SIZE")) {
      char path[FTP_CWD_SIZE];
      if (strlen(parameters) == 0)
        client.println("501 No file name");
      else if (makePath(path)) {
        file = _ptrSd->open(path, FILE_READ);
        if (!file)
          client.println("450 Can't open " + String(parameters));
        else {
          client.println("213 " + String(file.size()));
          file.close();
        }
      }
    }

    //
    //  SITE - System command
    //
    else if (!strcmp(command, "SITE")) {
      client.println("500 Unknown SITE command " + String(parameters));
    }

    //
    //  Unrecognized commands ...
    //
    else {
      client.println("500 Unknown command");
    }
    return true;
  }

  boolean dataConnect() {

    unsigned long startTime = millis();
    // wait 5 seconds for a data connection
    if (!data.connected()) {
      while (!dataServer.hasClient() && millis() - startTime < 10000) {
        //delay(100);
        //         yield();
      }
      if (dataServer.hasClient()) {
        data.stop();
        data = dataServer.available();
#ifdef FTP_DEBUG
        Serial.println("ftpdataserver client....");
#endif
      }
    }
    return data.connected();
  }

  boolean doRetrieve() {

    if (data.connected()) {
      int16_t nb = file.readBytes(buf, FTP_BUF_SIZE);
      if (nb > 0) {
        data.write((uint8_t *)buf, nb);
        bytesTransfered += nb;
        return true;
      }
    }
    closeTransfer();
    return false;
  }

  boolean doStore() {

    if (data.connected()) {
      // Avoid blocking by never reading more bytes than are available
      int navail = data.available();
      if (navail <= 0) return true;
      // And be sure not to overflow buf.
      if (navail > FTP_BUF_SIZE) navail = FTP_BUF_SIZE;
      int16_t nb = data.read((uint8_t *)buf, navail);
      // int16_t nb = data.readBytes((uint8_t*) buf, FTP_BUF_SIZE );
      if (nb > 0) {
        // Serial.println( millis() << " " << nb << endl;
        file.write((uint8_t *)buf, nb);
        bytesTransfered += nb;
      }
      return true;
    }
    closeTransfer();
    return false;
  }

  void closeTransfer() {

    uint32_t deltaT = (int32_t)(millis() - millisBeginTrans);
    if (deltaT > 0 && bytesTransfered > 0) {
      client.println("226-File successfully transferred");
      client.println("226 " + String(deltaT) + " ms, " + String(bytesTransfered / deltaT) + " kbytes/s");
    } else
      client.println("226 File successfully transferred");

    file.close();
    data.stop();
  }

  void abortTransfer() {

    if (transferStatus > 0) {
      file.close();
      data.stop();
      client.println("426 Transfer aborted");
#ifdef FTP_DEBUG
      Serial.println("Transfer aborted!");
#endif
    }
    transferStatus = 0;
  }

  boolean makePath(char *fullName) {

    return makePath(fullName, parameters);
  }

  boolean makePath(char *fullName, char *param) {

    if (param == NULL)
      param = parameters;

    // Root or empty?
    if (strcmp(param, "/") == 0 || strlen(param) == 0) {
      strcpy(fullName, "/");
      return true;
    }
    // If relative path, concatenate with current dir
    if (param[0] != '/') {
      strcpy(fullName, cwdName);
      if (fullName[strlen(fullName) - 1] != '/')
        strncat(fullName, "/", FTP_CWD_SIZE);
      strncat(fullName, param, FTP_CWD_SIZE);
    } else
      strcpy(fullName, param);
    // If ends with '/', remove it
    uint16_t strl = strlen(fullName) - 1;
    if (fullName[strl] == '/' && strl > 1)
      fullName[strl] = 0;
    if (strlen(fullName) < FTP_CWD_SIZE)
      return true;

    client.println("500 Command line too long");
    return false;
  }

  uint8_t getDateTime(uint16_t *pyear, uint8_t *pmonth, uint8_t *pday,
                      uint8_t *phour, uint8_t *pminute, uint8_t *psecond) {
    char dt[15];

    // Date/time are expressed as a 14 digits long string
    //   terminated by a space and followed by name of file
    if (strlen(parameters) < 15 || parameters[14] != ' ')
      return 0;
    for (uint8_t i = 0; i < 14; i++)
      if (!isdigit(parameters[i]))
        return 0;

    strncpy(dt, parameters, 14);
    dt[14] = 0;
    *psecond = atoi(dt + 12);
    dt[12] = 0;
    *pminute = atoi(dt + 10);
    dt[10] = 0;
    *phour = atoi(dt + 8);
    dt[8] = 0;
    *pday = atoi(dt + 6);
    dt[6] = 0;
    *pmonth = atoi(dt + 4);
    dt[4] = 0;
    *pyear = atoi(dt);
    return 15;
  }

  char *makeDateTimeStr(char *tstr, uint16_t date, uint16_t time) {

    sprintf(tstr, "%04u%02u%02u%02u%02u%02u",
            ((date & 0xFE00) >> 9) + 1980, (date & 0x01E0) >> 5, date & 0x001F,
            (time & 0xF800) >> 11, (time & 0x07E0) >> 5, (time & 0x001F) << 1);
    return tstr;
  }

  int8_t readChar() {

    int8_t rc = -1;

    if (client.available()) {
      char c = client.read();
      // char c;
      // client.readBytes((uint8_t*) c, 1);
#ifdef FTP_DEBUG
      Serial.print(c);
#endif
      if (c == '\\')
        c = '/';
      if (c != '\r') {
        if (c != '\n') {
          if (iCL < FTP_CMD_SIZE)
            cmdLine[iCL++] = c;
          else
            rc = -2;  //  Line too long
        } else {
          cmdLine[iCL] = 0;
          command[0] = 0;
          parameters = NULL;
          // empty line?
          if (iCL == 0)
            rc = 0;
          else {
            rc = iCL;
            // search for space between command and parameters
            parameters = strchr(cmdLine, ' ');
            if (parameters != NULL) {
              if (parameters - cmdLine > 4)
                rc = -2;  // Syntax error
              else {
                strncpy(command, cmdLine, parameters - cmdLine);
                command[parameters - cmdLine] = 0;

                while (*(++parameters) == ' ')
                  ;
              }
            } else if (strlen(cmdLine) > 4)
              rc = -2;  // Syntax error.
            else
              strcpy(command, cmdLine);
            iCL = 0;
          }
        }
      }
      if (rc > 0)
        for (uint8_t i = 0; i < strlen(command); i++)
          command[i] = toupper(command[i]);
      if (rc == -2) {
        iCL = 0;
        client.println("500 Syntax error");
      }
    }
    return rc;
  }

  IPAddress dataIp;  // IP address of client for data
  WiFiClient client;
  WiFiClient data;

  File file;

  boolean dataPassiveConn;
  uint16_t dataPort;
  char buf[FTP_BUF_SIZE];      // data buffer for transfers
  char cmdLine[FTP_CMD_SIZE];  // where to store incoming char from client
  char cwdName[FTP_CWD_SIZE];  // name of current directory
  char command[5];             // command sent by client
  boolean rnfrCmd;             // previous command was RNFR
  char *parameters;            // point to begin of parameters sent by client
  uint16_t iCL;                // pointer to cmdLine next incoming char
  int8_t cmdStatus,            // status of ftp command connexion
    transferStatus;            // status of ftp data transfer
  uint32_t millisTimeOut,      // disconnect after 5 min of inactivity
    millisDelay,
    millisEndConnection,  //
    millisBeginTrans,     // store time of beginning of a transaction
    bytesTransfered;      //
  String _FTP_USER;
  String _FTP_PASS;

  SdFat32 *_ptrSd;

};

#endif
