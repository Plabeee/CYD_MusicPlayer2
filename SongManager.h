/*
 * An instance of this class is used to manage
 * the Bluetooth connection and the MP3 song playing processes.
 *
 * This code is based upon three libraries written by Phil Schatzmann:
 * They are:
 *   1. ESP32-A2DP
 *   2. arduino-audio-tools
 *   3. arduino-libhelix
 *
 * Written by: Craig A. Lindley and Phil Schatzmann
 * Last Update: 11/06/2025
*/

#pragma once

// Remove this to find issues regarding mp3 decoding
#define HELIX_LOGGING_ACTIVE false
#define USE_SDFAT 1
#define DEFAULT_VOLUME 0.5

#include "AudioTools.h"
#include "AudioTools/Disk/SDDirect.h"

#include "MP3AudioPlayer.h"

#include "AudioSourceSDFAT.h"
#include "AudioTools/AudioLibs/A2DPStream.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

// Source doesn't control which song is played, the player does now
AudioSourceSDFAT source("", "");
A2DPStream out;
MP3DecoderHelix decoder;
MP3AudioPlayer player(source, out, decoder);

class SongManager {
public:

  void begin(SdFat32 *ptrSd) {

    source.setSd(ptrSd);

    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Error);

    // Setup output to connect to a Bluetooth Speaker
    // By not specifing cfg.name we will connect to first available BT device
    auto cfg = out.defaultConfig(TX_MODE);
    // cfg.name = "VHM-314";
    out.begin(cfg);

    currentVolume = DEFAULT_VOLUME;

    player.setVolume(currentVolume);
    player.begin();
  }

  // Determine if BT device has connected or not
  bool btConnected() {
    return out.isConnected();
  }

  // Volume 0.0 to 1.0
  bool setVolume(float volume) {
    currentVolume = volume;
    return player.setVolume(currentVolume);
  }

  void volumeUp() {
    if (currentVolume < 1.0) {
      currentVolume += 0.1;
      player.setVolume(currentVolume);
    }
  }

  void volumeDown() {
    if (currentVolume >= 0.1) {
      currentVolume -= 0.1;
      player.setVolume(currentVolume);
    }
  }

  // Plays the song specified with the full path on the SD card
  bool playSong(const char *path) {
    return player.playMP3(path);
  }

  void stopSong() {
    player.setActive(false);
  }

  void resume() {
    player.setActive(true);
  }

  bool isActive() {
    return player.isActive();
  }

  // This needs to be called in the Arduino loop() function
  // as fast as possible
  void loop() {
    player.copy();
  }

protected:

  float currentVolume;
};
