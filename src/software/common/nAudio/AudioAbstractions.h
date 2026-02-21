#pragma once
// Audios are much more simpler than graphics objects, the main focus here is to get basic sounds working and then advancing the driver class to be industry capable
// like allowing audio manipulation like reverb, all of those things while keeping it on 1 platform.
// Since audios are simple, this device class will be only one and anything that is os related will be headered
// This class won't use any audio libraries and stick to OS level sound design for control
#include "Utils.h"
#include <cstdint>
#include <vector>
#ifdef WIN32
#include <Audioclient.h>
#endif

enum class SampleType { INT16, INT24, INT32, FLOAT32 };

struct AudioMixerFormat {
    SampleType type;
    uint32_t sampleRate;
    uint16_t channels;
    uint16_t bitsPerSample;
    uint32_t bytesPerFrame;

    AudioMixerFormat() : type(SampleType::INT16), sampleRate(44100),
        channels(2), bitsPerSample(16), bytesPerFrame(4) {
    }

#ifdef WIN32
    AudioMixerFormat(const WAVEFORMATEX* fmt) {
        sampleRate = fmt->nSamplesPerSec;
        channels = fmt->nChannels;
        bitsPerSample = fmt->wBitsPerSample;
        bytesPerFrame = fmt->nBlockAlign;

        if (fmt->wFormatTag == WAVE_FORMAT_PCM)
            type = SampleType::INT16; // default int PCM
        else if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            auto ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(fmt);
            if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) type = SampleType::INT16;
            else if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) type = SampleType::FLOAT32;
            else type = SampleType::INT16; // fallback
        }
        else {
            type = SampleType::INT16; // fallback
        }
    }
#endif
};


struct PCMFormat {
    

    SampleType type = SampleType::INT16;
    uint32_t sampleRate = 44100;
    uint16_t channels = 2;
    uint16_t bitsPerSample = 16;

    uint32_t GetBytesPerFrame() const {
        return channels * bitsPerSample / 8;
    }

    uint32_t GetAvgBytesPerSec() const {
        return sampleRate * GetBytesPerFrame();
    }
};

struct AudioStream {
    void* data = nullptr;
    size_t totalBytes = 0;
    size_t bytesWritten = 0;
    PCMFormat format;  
    bool finished = false;
    float volume;

    ~AudioStream() {
        if (data) {
            delete data;
            data = nullptr;
        }
    }
};

struct IAudioDeviceConfig {
    // ------------------------
    // General configuration
    // ------------------------
    bool followMixerConfig = true;   // Automatically follow system mixer settings (sample rate, channels)

    // ------------------------
    // Format configuration
    // ------------------------
    int sampleRate = 44100;          // Sample rate in Hz (44.1kHz default)
    int channels = 2;                // Number of channels (1=mono, 2=stereo)
    int bitsPerSample = 16;          // Bit depth (16-bit PCM default)

    enum class SampleFormat {
        INT16,
        FLOAT32
    } sampleFormat = SampleFormat::INT16;  // Sample type

    // ------------------------
    // Audio mode
    // ------------------------
    enum class AudioMode {
        SHARED,     // Shared with other apps, goes through system mixer
        EXCLUSIVE   // Exclusive access to hardware, low-latency
    } mode = AudioMode::SHARED;

    // ------------------------
    // Buffer / latency
    // ------------------------
    uint32_t bufferDuration = 300000; // 100-ns units (e.g., 300000 = 30ms)
    uint32_t bufferFrameCount = 0;    // Optionally precompute or override

    // ------------------------
    // Advanced options
    // ------------------------
    bool enableHardwareMixing = false;    // Use hardware mixing if supported
    bool enableVolumeControl = true;      // Allow volume control on this device
    bool enableResampling = true;         // Allow sample-rate conversion if format doesn't match hardware
    bool allowDeviceSwitch = true;        // Automatically switch if default device changes

    // ------------------------
    // Optional DSP / effects hooks
    // ------------------------
    bool applyGlobalGain = true;          // Whether the engine applies master volume/gain
    float masterVolume = 1.0f;            // 0.0 = mute, 1.0 = full volume
};


enum class AudioDeviceUsecase {
    Any,
    Headphones,
    Earphones,
    Speakers,
    ExternalInterface, // e.g., USB audio interface
    Microphone // if supporting input later
};

struct AudioDeviceProperties {
    int sampleRate;          // Native sample rate of the device
    int channels;            // Number of channels supported
    int bitsPerSample;       // Native bit depth
    bool supportsFloat;      // Can handle float32 audio
    AudioDeviceUsecase usecase; // Type of device
    bool supportsExclusive;  // Exclusive mode supported (if OS supports)
    bool supportsHardwareMixing; // Can do internal device mixing
    // Optional additional flags for advanced capabilities
};

class IAudioDevice {
public:
    virtual ~IAudioDevice() = default;
    virtual bool Initialize() { return false; };
    virtual AudioStream* QueueStream(const void* data, size_t numBytes, const PCMFormat& format) { return nullptr; };
    virtual AudioStream* QueueStream(AudioStream* stream) { return nullptr; };
    virtual bool RemoveStream(const AudioStream* stream) { return false; };
    virtual void ClearAllStreams() {};
    virtual void Pause() {};
    virtual void Unpause() {};
    virtual bool Paused() { return false; };
    virtual void UpdateFrame() {};
    virtual void UpdateFrameAsAudioThread() {};
    virtual void Shutdown() {};
    virtual std::vector<std::wstring> ListPlaybackDevices() { return std::vector<std::wstring>(); };
    virtual std::vector<float>* GetMixerBuffer() { return nullptr; }
    virtual AudioMixerFormat GetMixerFormat() const { return AudioMixerFormat(); }
    virtual u32 GetMixerFramesPlayed() const { return 0; }
};
