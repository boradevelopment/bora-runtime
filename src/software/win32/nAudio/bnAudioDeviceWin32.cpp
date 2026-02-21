#if WIN32
#include "bnAudioDeviceWin32.h"

#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <atomic>

std::wstring GetDeviceId(IMMDevice* device)
{
    if (!device) return L"";

    LPWSTR deviceID = nullptr;
    HRESULT hr = device->GetId(&deviceID);
    if (FAILED(hr) || !deviceID)
        return L"";

    std::wstring result = deviceID;  // copy into std::wstring
    CoTaskMemFree(deviceID);          // free the memory returned by GetId
    return result;
}


std::wstring GetDefaultAudioDeviceId()
{
    CoInitialize(nullptr);

    IMMDeviceEnumerator* pEnum = nullptr;
    IMMDevice* pDevice = nullptr;
    LPWSTR deviceId = nullptr;
    std::wstring result;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, IID_PPV_ARGS(&pEnum)
    );

    if (SUCCEEDED(hr))
    {
        hr = pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        if (SUCCEEDED(hr))
        {
            hr = pDevice->GetId(&deviceId);
            if (SUCCEEDED(hr) && deviceId)
            {
                result = deviceId;
                CoTaskMemFree(deviceId);
            }
            pDevice->Release();
        }
        pEnum->Release();
    }

    CoUninitialize();
    return result;
}


void Convert16BitToFloat(const int16_t* src, size_t numSamples, float* output) {
    for (size_t i = 0; i < numSamples; ++i) {
        output[i] = src[i] / 32768.0f; // range [-1.0, 1.0]
    }
}

// src: pointer to original 16-bit PCM
// numSamples: total number of input samples (not bytes)
// srcRate: WAV sample rate
// dstRate: device sample rate
// channels: number of channels
// output: float vector to store resampled data

void bnAudioDeviceWin32::HandleAudioDeviceError(HRESULT hr)
{
    if (hr == AUDCLNT_E_DEVICE_INVALIDATED ||
        hr == AUDCLNT_E_RESOURCES_INVALIDATED ||
        hr == AUDCLNT_E_ENDPOINT_OFFLOAD_NOT_CAPABLE)
    {
        // Device lost or disconnected.
        printf("[Audio] Device disconnected or invalidated.\n");
        ResetAudioDevice();
        CoUninitialize();
        Initialize();
    }
    else {
        printf("[Audio] Unexpected WASAPI error: 0x%08X\n", hr);
    }
}

bool bnAudioDeviceWin32::Initialize()
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&enumerator)
    );
    if (FAILED(hr) || !enumerator) return false;

    // Select device
    ComPtr<IMMDevice> device;
    if (deviceID.empty()) {
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (FAILED(hr) || !device) return false;
        deviceID = GetDeviceId(device.Get());
    }
    else {
        hr = enumerator->GetDevice(deviceID.c_str(), &device);
        if (FAILED(hr) || !device) {
            hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
            if (FAILED(hr) || !device) return false;
        }
    }

    // Activate IAudioClient
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &audioClient);
    if (FAILED(hr) || !audioClient) return false;

    // Get device mix format
    WAVEFORMATEX* deviceMixFormat = nullptr;
    hr = audioClient->GetMixFormat(&deviceMixFormat);
    if (FAILED(hr) || !deviceMixFormat) return false;
    ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(deviceMixFormat);

    // Modify mixFormat according to config (if not following system mixer)
    if (!config.followMixerConfig) {
        deviceMixFormat->nChannels = static_cast<WORD>(config.channels);
        deviceMixFormat->nSamplesPerSec = config.sampleRate;
        deviceMixFormat->wBitsPerSample = static_cast<WORD>(config.bitsPerSample);
        deviceMixFormat->nBlockAlign = deviceMixFormat->nChannels * (deviceMixFormat->wBitsPerSample / 8);
        deviceMixFormat->nAvgBytesPerSec = deviceMixFormat->nSamplesPerSec * deviceMixFormat->nBlockAlign;

        if (config.sampleFormat == IAudioDeviceConfig::SampleFormat::FLOAT32) {
            deviceMixFormat->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        }
        else {
            deviceMixFormat->wFormatTag = WAVE_FORMAT_PCM;
        }
    }

    // Determine AUDCLNT_SHAREMODE
    bool shareMode = (config.mode == IAudioDeviceConfig::AudioMode::SHARED) ? true : false;

    // Initialize audio client
    REFERENCE_TIME bufferDuration = config.bufferDuration; // 100-ns units
    hr = audioClient->Initialize(
        (config.mode == IAudioDeviceConfig::AudioMode::SHARED) ? AUDCLNT_SHAREMODE_SHARED : AUDCLNT_SHAREMODE_EXCLUSIVE,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        bufferDuration,
        0,
        deviceMixFormat,
        nullptr
    );
    if (FAILED(hr)) return false;

    // Get the render client and buffer size
    hr = audioClient->GetService(IID_PPV_ARGS(&renderClient));
    if (FAILED(hr) || !renderClient) return false;

    hr = audioClient->GetBufferSize(&bufferFrameCount);
    if (FAILED(hr)) return false;

    audioEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    audioClient->SetEventHandle(audioEvent);

    mixFormat = deviceMixFormat;
    Unpause();
    
    return true;
}

AudioStream* bnAudioDeviceWin32::QueueStream(const void* data, size_t numBytes, const PCMFormat& format)
{
    auto stream = new AudioStream();
    stream->totalBytes = numBytes;
    stream->bytesWritten = 0;
    stream->format = format;

    // Never trust the user, i should probably do mem_s
    auto buffer = new uint8_t[numBytes];
    std::memcpy(buffer, data, numBytes);
    stream->data = buffer;


    activeStreams.push_back(stream);
    return stream;
}

AudioStream* bnAudioDeviceWin32::QueueStream(AudioStream* stream)
{
    if (!stream) return nullptr;

    stream->bytesWritten = 0;
    stream->finished = false;

    activeStreams.push_back(stream);
    return stream;
}

std::vector<std::wstring> bnAudioDeviceWin32::ListPlaybackDevices()
{
    std::vector<std::wstring> devices;
    CoInitialize(nullptr);

    IMMDeviceEnumerator* pEnum = nullptr;
    IMMDeviceCollection* pCollection = nullptr;

    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, IID_PPV_ARGS(&pEnum)
    );

    if (FAILED(hr)) return devices;

    // eRender = playback devices, DEVICE_STATE_ACTIVE only
    hr = pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) { pEnum->Release(); return devices; }

    UINT count = 0;
    pCollection->GetCount(&count);
    for (UINT i = 0; i < count; ++i)
    {
        IMMDevice* pDevice = nullptr;
        if (SUCCEEDED(pCollection->Item(i, &pDevice)))
        {
            IPropertyStore* pStore = nullptr;
            if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pStore)))
            {
                PROPVARIANT name;
                PropVariantInit(&name);
                if (SUCCEEDED(pStore->GetValue(PKEY_Device_FriendlyName, &name)))
                {
                    devices.push_back(name.pwszVal);
                }
                PropVariantClear(&name);
                pStore->Release();
            }
            pDevice->Release();
        }
    }

    pCollection->Release();
    pEnum->Release();

    return devices;
}

bool bnAudioDeviceWin32::RemoveStream(const AudioStream* stream)
{
    auto it = std::find(activeStreams.begin(), activeStreams.end(), stream);
    if (it != activeStreams.end()) {
        activeStreams.erase(it);
        delete stream;
        return true;
    }
    return false;
}

void bnAudioDeviceWin32::ResetAudioDevice()
{
    if (audioClient) audioClient->Stop();
    audioClient.Reset();
    renderClient.Reset();
}

void bnAudioDeviceWin32::ClearAllStreams()
{
    for (auto s : activeStreams) s->finished = true;
    activeStreams.clear();
    activeStreams.shrink_to_fit();
}

void bnAudioDeviceWin32::UpdateFrame()
{
    if (!audioClient || !renderClient || activeStreams.empty()) return;

    // Confirm if using the current audio device, if it isnt, change since we are using the whatevertheuserchooseontheos config coption
    std::wstring defaultId = GetDefaultAudioDeviceId();
    if (defaultId != deviceID)
    {
        // Switch to the new device
        Pause();
        deviceID = L"";
        ResetAudioDevice();
        if (Initialize()) {
            //Unpause();
        }
        else {
            HandleAudioDeviceError(AUDCLNT_E_DEVICE_INVALIDATED);
            return;
        }

        //
    }

    if (!renderClient || !audioClient) {
        HandleAudioDeviceError(AUDCLNT_E_DEVICE_INVALIDATED);
    }

    // Unpause();

    if (Paused()) return;

    UINT32 padding = 0;
    audioClient->GetCurrentPadding(&padding);
    UINT32 framesAvailable = bufferFrameCount - padding;
    if (framesAvailable == 0) return;

    BYTE* bufferBytes = nullptr;
    HRESULT hr = renderClient->GetBuffer(framesAvailable, &bufferBytes);
    if (FAILED(hr) || !bufferBytes) {
        HandleAudioDeviceError(hr);
        return;
    }


    // Ensure mixBuffer is large enough
    const size_t neededSize = framesAvailable * mixFormat->nChannels;
    if (mixBuffer.size() < neededSize)
        mixBuffer.resize(neededSize);

    // Clear it before mixing
    std::fill(mixBuffer.begin(), mixBuffer.begin() + neededSize, 0.0f);

    // Mix each stream
    for (auto it = activeStreams.begin(); it != activeStreams.end();) {
        AudioStream* stream = *it;
        uint32_t bytesLeft = stream->totalBytes - stream->bytesWritten;
        uint32_t framesLeft = bytesLeft / mixFormat->nBlockAlign;
        uint32_t framesToCopy = std::min(framesAvailable, framesLeft);

        if (stream->format.type == SampleType::FLOAT32) {
            float* src = reinterpret_cast<float*>(const_cast<void*>(stream->data)) + (stream->bytesWritten / sizeof(float));
            for (UINT32 f = 0; f < framesToCopy; ++f) {
                for (UINT32 c = 0; c < mixFormat->nChannels; ++c) {
                    mixBuffer[f * mixFormat->nChannels + c] += src[f * stream->format.channels + c];
                }
            }
        }
        else { // INT16
            int16_t* src = reinterpret_cast<int16_t*>(const_cast<void*>(stream->data)) + (stream->bytesWritten / 2);
            for (UINT32 f = 0; f < framesToCopy; ++f) {
                for (UINT32 c = 0; c < mixFormat->nChannels; ++c) {
                    mixBuffer[f * mixFormat->nChannels + c] += src[f * stream->format.channels + c] / 32768.0f;
                }
            }
        }

        stream->bytesWritten += framesToCopy * stream->format.GetBytesPerFrame();

        if (stream->bytesWritten >= stream->totalBytes) {
            stream->finished = true;
            it = activeStreams.erase(it);
        }
        else {
            ++it;
        }
    }
    

    // Convert to device format
    if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
        memcpy(bufferBytes, mixBuffer.data(), framesAvailable * mixFormat->nBlockAlign);
    }
    else {
        int16_t* dst = reinterpret_cast<int16_t*>(bufferBytes);
        for (size_t i = 0; i < neededSize; ++i) {
            float v = mixBuffer[i];
            v = std::max(-1.0f, std::min(1.0f, v));
            dst[i] = static_cast<int16_t>(v * 32767.0f);
        }
    }

    hr = renderClient->ReleaseBuffer(framesAvailable, 0);
    if (FAILED(hr)) {
        HandleAudioDeviceError(hr);
        return;
    }
    else {
        totalFramesWritten += framesAvailable;
    }
}

void bnAudioDeviceWin32::UpdateFrameAsAudioThread() {
    DWORD waitResult = WaitForSingleObject(audioEvent, 200); // timeout just in case
    if (waitResult != WAIT_OBJECT_0) return;
    UpdateFrame();
}


void bnAudioDeviceWin32::Shutdown()
{
    ClearAllStreams();
    if (audioEvent) CloseHandle(audioEvent);
    audioEvent = nullptr;
    ResetAudioDevice();
    CoUninitialize();

}

void bnAudioDeviceWin32::Pause()
{
    if (!pause) {
        pause = true;
        audioClient->Stop();
    }
}

void bnAudioDeviceWin32::Unpause()
{
    if (pause) {
    pause = false;
    audioClient->Start();
    }
}

bool bnAudioDeviceWin32::Paused()
{
    return pause;
}

std::vector<float>* bnAudioDeviceWin32::GetMixerBuffer()
{
    return &mixBuffer;
}

#endif