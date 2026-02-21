// Apart of the BORA Runtime Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.

#include "IconManager.h"
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "windowscodecs.lib")


ULONG_PTR IconManager::gdiplusToken;

#if WIN32
HRESULT WICConvertBitmapSourceToHBITMAP(
        IWICBitmapSource* pBitmapSource,
        HBITMAP* phBitmap,
        UINT& rWidth,
        UINT& rHeight
) {
    if (!pBitmapSource || !phBitmap) return E_INVALIDARG;
    *phBitmap = nullptr;

    IWICImagingFactory* pFactory = nullptr;
    IWICFormatConverter* pConverter = nullptr;
    IWICBitmapSource* pConvertedSource = nullptr;

    HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&pFactory)
    );

    // Convert to 32bppPBGRA
    if (SUCCEEDED(hr)) {
        hr = pFactory->CreateFormatConverter(&pConverter);
    }

    if (SUCCEEDED(hr)) {
        hr = pConverter->Initialize(
                pBitmapSource,
                GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone,
                nullptr, 0.f,
                WICBitmapPaletteTypeCustom
        );
    }

    if (SUCCEEDED(hr)) {
        pConvertedSource = pConverter;
        pConvertedSource->AddRef();
    }

    UINT width = 0, height = 0;
    if (SUCCEEDED(hr)) {
        hr = pConvertedSource->GetSize(&width, &height);
    }

    // In order to automatically get the real size of the image without developer input.
    rWidth = width;
    rHeight = height;

    // Create DIB Section
    BITMAPINFO bminfo = {};
    bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bminfo.bmiHeader.biWidth = width;
    bminfo.bmiHeader.biHeight = -(LONG)height; // Top-down bitmap
    bminfo.bmiHeader.biPlanes = 1;
    bminfo.bmiHeader.biBitCount = 32;
    bminfo.bmiHeader.biCompression = BI_RGB;

    void* pvImageBits = nullptr;
    if (SUCCEEDED(hr)) {
        *phBitmap = CreateDIBSection(
                nullptr, &bminfo, DIB_RGB_COLORS, &pvImageBits, nullptr, 0
        );
        if (!*phBitmap) hr = HRESULT_FROM_WIN32(GetLastError());
    }

    // Copy pixels
    if (SUCCEEDED(hr)) {
        const UINT stride = width * 4;
        const UINT cbBufferSize = stride * height;
        hr = pConvertedSource->CopyPixels(
                nullptr, stride, cbBufferSize, (BYTE*)pvImageBits
        );
    }

    // Cleanup
    if (pConvertedSource) pConvertedSource->Release();
    if (pConverter) pConverter->Release();
    if (pFactory) pFactory->Release();

    return hr;
}


HICON LoadNoGDIIcon(const BYTE* data, size_t size) {
    IWICImagingFactory* pFactory = nullptr;
    IWICStream* pStream = nullptr;
    IWICBitmapDecoder* pDecoder = nullptr;
    IWICBitmapFrameDecode* pFrame = nullptr;
    IWICFormatConverter* pConverter = nullptr;
    HBITMAP hBmp = nullptr;
    HICON hIcon = nullptr;
    ICONINFO iconInfo{};
    UINT targetWidth, targetHeight;

    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&pFactory))))
        return nullptr;

    if (FAILED(pFactory->CreateStream(&pStream))) goto cleanup;
    if (FAILED(pStream->InitializeFromMemory((WICInProcPointer)data, (DWORD)size))) goto cleanup;
    if (FAILED(pFactory->CreateDecoderFromStream(pStream, nullptr, WICDecodeMetadataCacheOnLoad, &pDecoder))) goto cleanup;
    if (FAILED(pDecoder->GetFrame(0, &pFrame))) goto cleanup;
    if (FAILED(pFactory->CreateFormatConverter(&pConverter))) goto cleanup;
    if (FAILED(pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppPBGRA,
                                      WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom))) goto cleanup;

    if (FAILED(WICConvertBitmapSourceToHBITMAP(pConverter, &hBmp, targetWidth, targetHeight))) goto cleanup;

    iconInfo.fIcon = TRUE;
    iconInfo.hbmColor = hBmp;
    iconInfo.hbmMask = CreateBitmap(targetWidth, targetHeight, 1, 1, nullptr);

    hIcon = CreateIconIndirect(&iconInfo);

    if (!hIcon) {
        DWORD err = GetLastError();
        printf("CreateIconIndirect failed: %lu\n", err);
    }

    // Cleanup GDI objects
    if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
    if (hBmp) DeleteObject(hBmp);

    cleanup:
    if (pConverter) pConverter->Release();
    if (pFrame) pFrame->Release();
    if (pDecoder) pDecoder->Release();
    if (pStream) pStream->Release();
    if (pFactory) pFactory->Release();

    return hIcon;
}

IconManager::IconManager() {
#if WIN32
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return;
    }

    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
#endif
}

IconManager::~IconManager() {
    GdiplusShutdown(gdiplusToken);
}

SysIcon IconManager::CreateIcon(sVec<u8> bytes) {
    if (bytes.empty()) return nullptr;

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
    if (!hMem) return nullptr;

    void* pMem = GlobalLock(hMem);
    memcpy(pMem, bytes.data(), bytes.size());
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
        bmp = nullptr;
    }

    if (!bmp) {
        // Uses WDI Codecs
        return LoadNoGDIIcon(bytes.data(), bytes.size());
    }

    HICON hIcon = nullptr;
    bmp->GetHICON(&hIcon);
    delete bmp;

    return hIcon;
}

SysIcon IconManager::CreateIcon(const u8 *imageData, size_t dataSize) {
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
        bmp = nullptr;
    }


    if (!bmp) {
        return LoadNoGDIIcon(imageData, dataSize);
    }

    // Convert to HICON
    HICON hIcon = nullptr;
    bmp->GetHICON(&hIcon);
    delete bmp;

    return hIcon;
}

static inline DWORD DwordAlign(DWORD n) {
    // Align to next multiple of 4 bytes
    return (n + 3u) & ~3u;
}

static std::vector<u8> ExtractIconImage(HICON hIcon, int& outWidth, int& outHeight) {
    ICONINFO ii{};
    if (!GetIconInfo(hIcon, &ii))
        return {};

    BITMAP bmColor{};
    BITMAP bmMask{};
    if (ii.hbmColor) GetObject(ii.hbmColor, sizeof(bmColor), &bmColor);
    if (ii.hbmMask)  GetObject(ii.hbmMask, sizeof(bmMask), &bmMask);

    const int width = ii.hbmColor ? bmColor.bmWidth : bmMask.bmWidth;
    const int height = ii.hbmColor ? bmColor.bmHeight : bmMask.bmHeight;
    if (width <= 0 || height <= 0) {
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
        if (ii.hbmMask)  DeleteObject(ii.hbmMask);
        return {};
    }
    outWidth = width;
    outHeight = height;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(nullptr);

    const DWORD xorStride = DwordAlign(width * 4);
    std::vector<BYTE> xorBits(xorStride * height, 0);
    GetDIBits(hdc, ii.hbmColor ? ii.hbmColor : ii.hbmMask, 0, height, xorBits.data(), &bmi, DIB_RGB_COLORS);

    const DWORD andStride = DwordAlign((DWORD)((width + 7) / 8));
    std::vector<BYTE> andBits(andStride * height, 0x00);

    // Build AND mask from alpha
    for (int y = 0; y < height; ++y) {
        BYTE* andRow = &andBits[y * andStride];
        const BYTE* xorRow = &xorBits[y * xorStride];
        for (int x = 0; x < width; ++x) {
            const BYTE a = xorRow[x * 4 + 3];
            if (a == 0) {
                andRow[x >> 3] |= (BYTE)(0x80 >> (x & 7));
            }
        }
    }

    ReleaseDC(nullptr, hdc);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    if (ii.hbmMask)  DeleteObject(ii.hbmMask);

    // Build final DIB block
    BITMAPINFOHEADER bih{};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = width;
    bih.biHeight = height * 2;
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = xorStride * height + andStride * height;

    std::vector<BYTE> imageData;
    imageData.resize(sizeof(bih) + bih.biSizeImage);
    memcpy(imageData.data(), &bih, sizeof(bih));
    memcpy(imageData.data() + sizeof(bih), xorBits.data(), xorStride * height);
    memcpy(imageData.data() + sizeof(bih) + xorStride * height, andBits.data(), andStride * height);

    return imageData;
}

bool IconManager::SaveIcon(SysIcon icon, const wchar_t *path, std::vector<std::pair<int, int>> sizes) {
    if (!icon) return false;

    if (sizes.empty()) {
        // get the current resolution of the icon and then push back it, then downscale it.
        ICONINFO ii{};
        if (!GetIconInfo(icon, &ii)) return false;

        BITMAP bmColor{};
        GetObject(ii.hbmColor, sizeof(bmColor), &bmColor);

        int cWidth = bmColor.bmWidth;
        int cHeight = bmColor.bmHeight;

        sizes.emplace_back( cWidth, cHeight );
        if(cWidth > 500 && cHeight > 500) {
            while (cWidth > 8 && cHeight > 8) {
                cWidth /= 2;
                cHeight /= 2;
                sizes.emplace_back(cWidth, cHeight);
                if (cWidth > 0 && cHeight > 0) {
                    sizes.emplace_back(cWidth, cHeight);
                }
            }
        }
    }

    ICONDIR dir{};
    dir.idReserved = 0;
    dir.idType = 1;
    dir.idCount = (WORD)sizes.size();

    std::vector<ICONDIRENTRY> entries(dir.idCount);
    std::vector<std::vector<BYTE>> images;

    DWORD offset = sizeof(ICONDIR) + sizeof(ICONDIRENTRY) * dir.idCount;

    int i = -1;
    for (const auto& [cW, cH] : sizes) {
        i++;
        auto resizedIcon = (HICON)CopyImage(icon, IMAGE_ICON, cW, cH, LR_COPYFROMRESOURCE);
        int w, h;
        std::vector<BYTE> imgData = ExtractIconImage(resizedIcon, w, h);
        DestroyIcon(resizedIcon);

        if (imgData.empty()) continue;

        ICONDIRENTRY entry{};
        entry.bWidth = (BYTE)(w >= 256 ? 0 : w);
        entry.bHeight = (BYTE)(h >= 256 ? 0 : h);
        entry.bColorCount = 0;
        entry.wPlanes = 1;
        entry.wBitCount = 32;
        entry.dwBytesInRes = (DWORD)imgData.size();
        entry.dwImageOffset = offset;

        offset += entry.dwBytesInRes;
        entries[i] = entry;
        images.push_back(std::move(imgData));
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    out.write((char*)&dir, sizeof(dir));
    out.write((char*)entries.data(), sizeof(ICONDIRENTRY) * entries.size());
    for (auto& img : images) {
        out.write((char*)img.data(), img.size());
    }

    return true;
}

#endif