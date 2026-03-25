#pragma once
#include "bskia/include/core/SkFontMgr.h"
#include "bskia/include/core/SkTypeface.h"
#include "bskia/include/core/SkTextBlob.h"
#ifdef WIN32
#include "bskia/include/ports/SkTypeface_win.h"
#elif defined(__APPLE__)
#include "bskia/include/ports/SkFontMgr_mac_ct.h"
#endif

class bnFontMgr
{
public:
    bnFontMgr();

    ~bnFontMgr();

    SkFont getFont(const char* family,
                   int size,
                   int weight = SkFontStyle::kNormal_Weight,
                   int width = SkFontStyle::kNormal_Width,
                   SkFontStyle::Slant slant = SkFontStyle::kUpright_Slant);
  
private:
    sk_sp<SkFontMgr> mgr;
};

