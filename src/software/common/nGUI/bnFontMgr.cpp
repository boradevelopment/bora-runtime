#include "bnFontMgr.h"

bnFontMgr::bnFontMgr()
{
#ifdef WIN32
    mgr = sk_sp<SkFontMgr>(SkFontMgr_New_DirectWrite());
#elif defined(__APPLE__)
    mgr = SkFontMgr_New_CoreText(nullptr); // todo?
#endif
}

bnFontMgr::~bnFontMgr() {
    mgr.reset();
}

SkFont bnFontMgr::getFont(const char* family, int size, int weight, int width, SkFontStyle::Slant slant)
{
    SkFontStyle style(weight, width, slant);
    sk_sp<SkTypeface> tf = sk_sp<SkTypeface>(mgr->matchFamilyStyleCharacter(family, style, nullptr, 0, U'日'));
    if (!tf) {
        // fallback to default system font
        tf = sk_sp<SkTypeface>(mgr->legacyMakeTypeface(nullptr, style));
    }

    SkFont font(tf, static_cast<SkScalar>(size));
    font.setEdging(SkFont::Edging::kSubpixelAntiAlias);
    font.setHinting(SkFontHinting::kFull);
    return font; // SkFont is lightweight, safe to return by value
}