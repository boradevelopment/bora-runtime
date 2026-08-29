#pragma once
#include <thread>
#include "bskia/include/core/SkSurface.h"
#include "bskia/include/core/SkCanvas.h"
#include "bskia/include/core/SkFont.h"
#include "bskia/include/core/SkImage.h"
#include "bskia/include/core/SkImageInfo.h"
#include "bskia/include/core/SkData.h"
#include "bskia/include/codec/SkCodec.h"
#ifdef WIN32
#include "win32Utils.h"
#endif

#ifdef BORA_UI_SUPPORT
inline sk_sp<SkImage> LoadImageFromMemory(const u8* data, size_t size) {
    if (!data || size == 0) return nullptr;

    auto codec = SkCodec::MakeFromData(SkData::MakeWithoutCopy(data, size));
    if (!codec) return nullptr;

    SkImageInfo info = SkImageInfo::MakeN32Premul(
        codec->dimensions().width(),
        codec->dimensions().height(),
        SkColorSpace::MakeSRGB()
    );

    std::vector<uint8_t> pixels(info.minRowBytes() * info.height());

    if (codec->getPixels(info, pixels.data(), info.minRowBytes()) != SkCodec::kSuccess) {
        return nullptr;
    }

    // Premultiply alpha manually as images look like shit without it
    for (int y = 0; y < info.height(); y++) {
        auto* row = reinterpret_cast<uint32_t*>(pixels.data() + y * info.minRowBytes());
        for (int x = 0; x < info.width(); x++) {
            auto* px = reinterpret_cast<uint8_t*>(&row[x]);
            uint8_t a = px[3];
            px[0] = px[0] * a / 255; // B
            px[1] = px[1] * a / 255; // G
            px[2] = px[2] * a / 255; // R
        }
    }

    SkPixmap pixmap(info, pixels.data(), info.minRowBytes());
    return SkImages::RasterFromPixmapCopy(pixmap);
}


inline std::vector<std::u16string> wrapText(const std::u16string& text, SkFont& font, float maxWidth) {
    std::vector<std::u16string> lines;
    std::u16string currentLine;
    std::u16string word;
    SkRect bounds;

    for (size_t i = 0; i <= text.size(); ++i) {
        char16_t c = (i < text.size()) ? text[i] : u' ';
        if (c == u' ' || c == u'\n' || i == text.size()) {
            // measure current line + word
            std::u16string testLine = currentLine + (currentLine.empty() ? u"" : u" ") + word;
            float width = font.measureText(testLine.c_str(), testLine.size() * sizeof(char16_t), SkTextEncoding::kUTF16, &bounds);

            if (width > maxWidth && !currentLine.empty()) {
                lines.push_back(currentLine);
                currentLine = word; // start new line
            }
            else {
                if (!currentLine.empty()) currentLine += u' ';
                currentLine += word;
            }

            word.clear();
            if (c == u'\n') {
                lines.push_back(currentLine);
                currentLine.clear();
            }
        }
        else {
            word += c;
        }
    }

    if (!currentLine.empty())
        lines.push_back(currentLine);

    return lines;
}
#endif

inline void KillThread(std::thread& thread) {
#ifdef WIN32
    TerminateThread(thread.native_handle(), 1);
#elif defined(__linux__)
    pthread_cancel(thread.native_handle());
#endif

if (thread.joinable()) {
        thread.join();
 }
}

inline double GetDeltaTime()
{
    using Clock = std::chrono::steady_clock;

    thread_local auto prevTime = Clock::now();
    thread_local bool initialized = false;

    if (!initialized) {
        prevTime = Clock::now();
        initialized = true;
        return 0.0; // First frame, no delta
    }

    auto currentTime = Clock::now();
    std::chrono::duration<double> delta = currentTime - prevTime;
    prevTime = currentTime;

    return delta.count();
}


inline std::u16string wstring_to_utf16(const std::wstring& ws) {
    return std::u16string(ws.begin(), ws.end());
};

