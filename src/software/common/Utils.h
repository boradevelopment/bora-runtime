#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <Shlwapi.h>
#include <thread>
#include "bskia/include/core/SkSurface.h"
#include "bskia/include/core/SkCanvas.h"
#include "bskia/include/core/SkPaint.h"
#include "bskia/include/core/SkFont.h"
#include "bskia/include/core/SkImage.h"
#include "bskia/include/core/SkImageInfo.h"
#include "bskia/include/core/SkData.h"
#include "bskia/include/core/SkStream.h"
#include "bskia/include/codec/SkCodec.h"

using namespace Gdiplus;

inline HICON CreateIconFromLogo(const u8* imageData, size_t dataSize) {
    if (!imageData || dataSize == 0) return nullptr;

    // Create IStream from memory buffer
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, dataSize);
    if (!hMem) return nullptr;

    void* pMem = GlobalLock(hMem);
    memcpy(pMem, imageData, dataSize);
    GlobalUnlock(hMem);

    IStream* pStream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(hMem, TRUE, &pStream))) {
        GlobalFree(hMem);
        return nullptr;
    }

    // Load bitmap from IStream
    Bitmap* bmp = Bitmap::FromStream(pStream);
    pStream->Release();

    if (!bmp || bmp->GetLastStatus() != Ok) {
        delete bmp;
        return nullptr;
    }

    // Convert to HICON
    HICON hIcon = nullptr;
    bmp->GetHICON(&hIcon);
    delete bmp;

    return hIcon;
}

inline sk_sp<SkImage> LoadImageFromMemory(const BYTE* data, size_t size) {
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
        uint32_t* row = reinterpret_cast<uint32_t*>(pixels.data() + y * info.minRowBytes());
        for (int x = 0; x < info.width(); x++) {
            uint8_t* px = reinterpret_cast<uint8_t*>(&row[x]);
            uint8_t a = px[3];
            px[0] = px[0] * a / 255; // B
            px[1] = px[1] * a / 255; // G
            px[2] = px[2] * a / 255; // R
        }
    }

    SkPixmap pixmap(info, pixels.data(), info.minRowBytes());
    return SkImages::RasterFromPixmapCopy(pixmap);
}


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
{ // threads shouldnt use the same things.
    thread_local LARGE_INTEGER prevTime = { 0 };
    thread_local double freq = 0.0;

    if (freq == 0.0) {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        freq = double(frequency.QuadPart);
        QueryPerformanceCounter(&prevTime);
        return 0.0; // first frame, no delta
    }

    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);

    double delta = double(currentTime.QuadPart - prevTime.QuadPart) / freq;
    prevTime = currentTime;

    return delta;
}


inline std::u16string wstring_to_utf16(const std::wstring& ws) {
    return std::u16string(ws.begin(), ws.end());
};

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
