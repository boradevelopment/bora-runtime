#pragma once
#include "PlatformInterfaces.h"
#include "AudioAbstractions.h"
#ifdef WIN32
#include "nAudio/bnAudioDeviceWin32.h"
#endif
struct AudioDeviceResult {
    IAudioDevice* device = nullptr;
    bool initialized = false;
    
};

inline AudioDeviceResult CreateAudioDevice(SysHandle& window, IAudioDeviceConfig* config)
{
    AudioDeviceResult result;


#ifdef _WIN32
    result.device = (IAudioDevice*)new bnAudioDeviceWin32(window, *config);
#endif

#ifdef __APPLE__
    result.device = (IAudioDevice*)new bnAudioDeviceDarwin(window, *config);
#endif

#ifdef __ANDROID__
    result.device = (IAudioDevice*)new bnAudioDeviceAndroid(window, *config);
#endif

#ifdef __linux__
    result.device = (IAudioDevice*)new bnAudioDeviceLinux(window, *config);
#endif

    if (result.device)
        result.initialized = result.device->Initialize();

    return result;
}
