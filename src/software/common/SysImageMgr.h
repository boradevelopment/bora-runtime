// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.

/* Common

 * FileName: IconManager.h
 * Title: Icon Manager
 * Author: Munashe Dirwayi
 * Purpose: Serve as a static class to create dedicated ICONS for the targetted operating

 * Compatibility: ?

 * Updates - ?
 * Known issues - ?
 */
#ifndef BORA_ICONMANAGER_H
#define BORA_ICONMANAGER_H

#if WIN32
#include <fstream>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <gdiplus.h>
using namespace Gdiplus;
typedef HICON SysIcon;
#endif

#pragma pack(push, 1)
struct ICONDIR {
    WORD idReserved; // 0
    WORD idType;     // 1 = icon
    WORD idCount;    // number of images
};
struct ICONDIRENTRY {
    BYTE  bWidth;        // 0 means 256
    BYTE  bHeight;       // 0 means 256
    BYTE  bColorCount;   // 0 if >= 8bpp
    BYTE  bReserved;     // 0
    WORD  wPlanes;       // 1 for icons
    WORD  wBitCount;     // 32 for 32bpp
    DWORD dwBytesInRes;  // size of image data
    DWORD dwImageOffset; // offset of image data
};
#pragma pack(pop)

class IconManager {
public:
    IconManager();
    ~IconManager();
public:
    static SysIcon CreateIcon(sVec<u8> bytes);
    static SysIcon CreateIcon(const u8* imageData, size_t dataSize);
    static bool SaveIcon(SysIcon icon, const wchar_t* path, std::vector<std::pair<int, int>> sizes = {});
#if WIN32 // Win32 exclusive
private:
    static ULONG_PTR gdiplusToken;
#endif

};


#endif //BORA_ICONMANAGER_H
