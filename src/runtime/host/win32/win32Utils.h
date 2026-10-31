// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: win32Utils.h
 * Purpose: ?
*/
#pragma once
#include "bskia/include/core/SkStream.h"
#include <windows.h>
#include <gdiplus.h>
#include <Shlwapi.h>
#include "bskia/include/core/SkPaint.h"

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