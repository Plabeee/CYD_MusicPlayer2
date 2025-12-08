#pragma once

#include "AudioTools/AudioCodecs/AudioCodecs.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "AudioTools/CoreAudio/BaseConverter.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioTools/CoreAudio/StreamCopy.h"
#include "AudioTools/CoreAudio/VolumeStream.h"
#include "AudioTools/Disk/AudioSource.h"
#include "AudioToolsConfig.h"

namespace audio_tools {

/**
 * @brief Implements a simple mp3 audio player which supports the following
 * commands:
 * - begin
 * - play
 * - stop
 * - next
 * - set Volume
 * @ingroup player
 * @author Phil Schatzmann and Craig A. Lindley
 * @copyright GPLv3
 */
class MP3AudioPlayer : public VolumeSupport {
public:

  /**
   * @brief Construct a new Audio Player object. The processing chain is
   * AudioSource -> Stream-copy -> EncodedAudioStream -> VolumeStream ->
   * Print
   *
   * @param source
   * @param output
   * @param decoder
   */
  MP3AudioPlayer(AudioSource &source, AudioStream &output, AudioDecoder &decoder) {
    TRACED();
    this->p_source = &source;
    this->p_decoder = &decoder;
    setOutput(output);
  }

  void setOutput(AudioStream &output) {
    if (p_decoder->isResultPCM()) {
      this->volume_out.setOutput(output);
      out_decoding.setOutput(&volume_out);
      out_decoding.setDecoder(p_decoder);
    } else {
      out_decoding.setOutput(&output);
      out_decoding.setDecoder(p_decoder);
    }
    this->p_final_print = nullptr;
    this->p_final_stream = &output;
  }

  // Initialize player dependent objects
  bool begin() {

    // initilaize volume
    if (current_volume == -1.0f) {
      setVolume(1.0f);
    } else {
      setVolume(current_volume);
    }

    // start dependent objects
    out_decoding.begin();
    p_source->begin();
    volume_out.begin();

    return true;
  }

  void end() {
    active = false;
    out_decoding.end();
    // remove any data in the decoder
    if (p_decoder != nullptr) {
      LOGI("reset codec");
      p_decoder->end();
      p_decoder->begin();
    }
  }

  /// (Re)defines the decoder
  void setDecoder(AudioDecoder &decoder) {
    this->p_decoder = &decoder;
    out_decoding.setDecoder(p_decoder);
  }

  /// starts / resumes the playing after calling stop(): same as setActive(true)
  void play() {
    setActive(true);
  }

  /// halts the playing: same as setActive(false)
  void stop() {
    setActive(false);
    writeEnd();
  }

  /// play an MP3 audio file
  /// @param path path to the audio file
  /// @return true if file was found; false otherwise
  bool playMP3(const char *path) {

    writeEnd();

    if (!setStream(p_source->selectStream(path))) {
      LOGW("Could not open file: %s", path);
      return false;
    }

    LOGI("Playing %s", path);

    bool result;

    if (p_input_stream != nullptr) {
      copier.begin(out_decoding, *p_input_stream);
      timeout = millis() + p_source->timeoutAutoNext();
      active = true;
      result = true;

      play();

    } else {
      LOGW("-> begin: no data found");
      active = false;
      result = false;
    }
    return result;
  }

  /// start selected input stream
  bool setStream(Stream *input) {
    end();
    out_decoding.begin();
    p_input_stream = input;
    if (p_input_stream != nullptr) {
      LOGD("open selected stream");
      copier.begin(out_decoding, *p_input_stream);
    }
    return p_input_stream != nullptr;
  }

  /// The same like start() / stop()
  void setActive(bool isActive) {
    active = isActive;
  }

  // Get active status
  bool isActive() {
    return active;
  }

  /// sets the volume - values need to be between 0.0 and 1.0
  bool setVolume(float volume) override {
    bool result = true;
    if (volume >= 0.0f && volume <= 1.0f) {
      if (abs(volume - current_volume) > 0.01f) {
        LOGI("setVolume(%f)", volume);
        volume_out.setVolume(volume);
        current_volume = volume;
      }
    } else {
      LOGE("setVolume value '%f' out of range (0.0 - 1.0)", volume);
      result = false;
    }
    return result;
  }

  /// Copies DEFAULT_BUFFER_SIZE (=1024 bytes) from the source to the decoder:
  /// Call this method in the loop.
  size_t copy() {
    return copy(copier.bufferSize());
  }

  /// Copies the indicated number of bytes from the source to the decoder: Call
  /// this method in the loop.
  size_t copy(size_t bytes) {
    size_t result = 0;
    if (active) {

      if (delay_if_full != 0 && ((p_final_print != nullptr && p_final_print->availableForWrite() == 0) || (p_final_stream != nullptr && p_final_stream->availableForWrite() == 0))) {
        // not ready to do anything - so we wait a bit
        delay(delay_if_full);
        return 0;
      }
      // handle sound
      result = copier.copyBytes(bytes);
      if (result > 0 || timeout == 0) {

        // reset timeout if we had any data
        timeout = millis() + p_source->timeoutAutoNext();
      }
      // Check timeout for song end
      checkForSongEnd();

      // return silence when there was no data
      if (result < bytes && silence_on_inactive) {
        writeSilence(bytes - result);
      }

    } else {
      // e.g. A2DP should still receive data to keep the connection open
      if (silence_on_inactive) {
        writeSilence(1024);
      }
    }
    return result;
  }

  /// If set to true the player writes 0 values instead of no data if the player
  /// is inactive
  void setSilenceOnInactive(bool active) {
    silence_on_inactive = active;
  }

  /// Checks if silence_on_inactive has been activated (default false)
  bool isSilenceOnInactive() {
    return silence_on_inactive;
  }

  /// Sends the requested bytes as 0 values to the output
  void writeSilence(size_t bytes) {
    if (p_final_print != nullptr) {
      p_final_print->writeSilence(bytes);
    } else if (p_final_stream != nullptr) {
      p_final_stream->writeSilence(bytes);
    }
  }

protected:
  bool active = false;
  bool silence_on_inactive = false;
  AudioSource *p_source = nullptr;
  VolumeStream volume_out;          // Volume control
  EncodedAudioOutput out_decoding;  // Decoding stream
  CopyDecoder no_decoder{ true };
  AudioDecoder *p_decoder = &no_decoder;
  Stream *p_input_stream = nullptr;
  AudioOutput *p_final_print = nullptr;
  AudioStream *p_final_stream = nullptr;
  StreamCopy copier;  // copies sound into i2s
  uint32_t timeout = 0;
  float current_volume = -1.0f;  // illegal value which will trigger an update
  int delay_if_full = 100;

  void checkForSongEnd() {
    if (p_final_stream != nullptr && p_final_stream->availableForWrite() == 0)
      return;

    if (p_input_stream == nullptr || millis() > timeout) {
      // Playback complete
      active = false;

      timeout = millis() + p_source->timeoutAutoNext();
    }
  }

  void writeEnd() {
    // Restart the decoder to make sure it does not contain any audio when we
    // continue
    p_decoder->begin();
  }
};

}  // namespace audio_tools
