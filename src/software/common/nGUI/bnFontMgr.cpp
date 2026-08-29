#include "bnFontMgr.h"
#include "nGUI/skia/interfaces/font/IGUIFontManager.h"

bnFontMgr::bnFontMgr()
{
    mgr = SkiaFontManager();
}

bnFontMgr::~bnFontMgr() {

}

ResourceHandle<IGUIFont> bnFontMgr::getFont(const char* family, int size, int weight, int width, GUIFontStyle::Slant slant)
{
   return mgr.getFont(family, size, {weight, width, slant});
    //
    // SkFontStyle style(weight, width, slant);
    // sk_sp<SkTypeface> tf = sk_sp<SkTypeface>(mgr->matchFamilyStyleCharacter(family, style, nullptr, 0, U'日'));
    // if (!tf) {
    //     // fallback to default system font
    //     tf = sk_sp<SkTypeface>(mgr->legacyMakeTypeface(nullptr, style));
    // }
    //
    // SkFont font(tf, static_cast<SkScalar>(size));
    // font.setEdging(SkFont::Edging::kSubpixelAntiAlias);
    // font.setHinting(SkFontHinting::kFull);
    // return font; // SkFont is lightweight, safe to return by value
}

ResourceHandle<IGUIFont> bnFontMgr::getFont(const char* family, int size, GUIFontStyle style)
{
    return mgr.getFont(family, size, style);
}

ResourceHandle<IGUIFontTextBlob> bnFontMgr::createTextBlob(const void* text, size_t byteLength, const IGUIFont* font,
    TextEncoding encoding)
{
    return mgr.createTextBlob(text, byteLength, font, encoding);
}

ResourceHandle<IGUIFontTextBlob> bnFontMgr::createTextBlob(const char* string, const IGUIFont* font,
    TextEncoding encoding)
{
    return mgr.createTextBlob(string, font, encoding);
}
