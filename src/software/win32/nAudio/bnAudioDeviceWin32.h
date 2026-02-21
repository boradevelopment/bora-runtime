#pragma once
#include "nAudio/AudioAbstractions.h"
#ifdef WIN32
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class bnAudioDeviceWin32;

class bnAudioDeviceWin32 : public IAudioDevice {
private:
    std::wstring deviceID;
    ComPtr<IAudioClient> audioClient;
    ComPtr<IAudioRenderClient> renderClient;
    WAVEFORMATEX* mixFormat = nullptr;
    WAVEFORMATEXTENSIBLE* ext = nullptr;
    u32 bufferFrameCount = 0;
    u32 totalFramesWritten = 0;

    SysHandle& windowHandle;
    IAudioDeviceConfig& config;
    std::atomic<bool> pause;
    std::vector<AudioStream*> activeStreams;
    std::vector<float> mixBuffer;
    void HandleAudioDeviceError(HRESULT hr);
    HANDLE audioEvent = nullptr;
public:
    bnAudioDeviceWin32(SysHandle& window, IAudioDeviceConfig& cfg)
        : windowHandle(window), config(cfg) {
    }
    ~bnAudioDeviceWin32() {

    }
    bool Initialize() override;
    AudioStream* QueueStream(const void* data, size_t numBytes, const PCMFormat& format) override;
    AudioStream* QueueStream(AudioStream* stream) override;
    std::vector<std::wstring> ListPlaybackDevices() override;
    bool RemoveStream(const AudioStream* stream) override;
    void ResetAudioDevice();
    void ClearAllStreams() override;
    void UpdateFrame() override;
    void UpdateFrameAsAudioThread() override;
    void Shutdown() override;
    void Pause() override;
    void Unpause() override;
    bool Paused() override;
    std::vector<float>* GetMixerBuffer() override;
    AudioMixerFormat GetMixerFormat() const {
        if (mixFormat)
            return AudioMixerFormat(mixFormat);
        return AudioMixerFormat(); // fallback default
    }
    u32 GetMixerFramesPlayed() const {
        if (!audioClient) return 0;
        UINT32 padding = 0;
        HRESULT hr = audioClient->GetCurrentPadding(&padding);

        if (SUCCEEDED(hr)) {
            // Get buffer size in frames
            UINT32 bufferFrameCount = 0;
            audioClient->GetBufferSize(&bufferFrameCount);

            // Calculate how many frames have been played (approx)
            UINT32 framesPlayed = totalFramesWritten - padding;
        }

        return padding;
    }
};

#else
class bnAudioDeviceWin32 : public IAudioDevice;
#endif