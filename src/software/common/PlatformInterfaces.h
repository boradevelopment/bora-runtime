#pragma once

enum class Platforms {
    NONE = -1,
    WINDOWS = 0,
    MACOS = 1,
    LINUX = 2,
    IOS = 3,
    ANDROID = 4,
};

enum class Kernals {
    NONE = -1,
    WINDOWS = 0,
    DARWIN = 1,
    LINUX = 2,
};

inline Platforms GetPlatform() {
#ifdef WIN32
    return Platforms::WINDOWS;
#elif defined(MACOS)
    return Platforms::MACOS;
#elif defined(LINUX)
    return Platforms::LINUX;
#elif defined(IOS)
    return Platforms::IOS;
#elif defined(ANDROID)
    return Platforms::ANDROID;
#else
    return Platforms::NONE;
#endif
}

inline Kernals GetKernel() {
#ifdef WIN32
    return Kernals::WINDOWS;
#elif defined(MACOS) || defined(IOS)
    return Kernals::DARWIN;
#elif defined(LINUX) || defined(ANDROID)
    return Kernals::LINUX;
#else
    return Kernals::NONE;
#endif
}
