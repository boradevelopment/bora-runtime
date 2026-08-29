#pragma once
#include "bskia/include/core/SkFontMgr.h"
#include "bskia/include/core/SkTypeface.h"
#include "bskia/include/core/SkTextBlob.h"
#include "nGUI/skia/abstractions/font/SkiaFontManager.h"
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

    ResourceHandle<IGUIFont> getFont(const char* family, int size, int weight, int width, GUIFontStyle::Slant slant);
    ResourceHandle<IGUIFont> getFont(const char* family, int size, GUIFontStyle style);


    /** Creates SkTextBlob with a single run.

        font contains attributes used to define the run text.

        When encoding is SkTextEncoding::kUTF8, SkTextEncoding::kUTF16, or
        SkTextEncoding::kUTF32, this function uses the default
        character-to-glyph mapping from the SkTypeface in font.  It does not
        perform typeface fallback for characters not found in the SkTypeface.
        It does not perform kerning or other complex shaping; glyphs are
        positioned based on their default advances.

        @param text        character code points or glyphs drawn
        @param byteLength  byte length of text array
        @param font        text size, typeface, text scale, and so on, used to draw
        @param encoding    text encoding used in the text array
        @return            SkTextBlob constructed from one run
    */
    virtual ResourceHandle<IGUIFontTextBlob> createTextBlob(const void* text, size_t byteLength, const IGUIFont* font, TextEncoding encoding = TextEncoding::kUTF8);

    /** Creates a TextBlob with a single run. string meaning depends on SkTextEncoding;
        by default, string is encoded as UTF-8.

        font contains attributes used to define the run text.

        When encoding is SkTextEncoding::kUTF8, SkTextEncoding::kUTF16, or
        SkTextEncoding::kUTF32, this function uses the default
        character-to-glyph mapping from the SkTypeface in font.  It does not
        perform typeface fallback for characters not found in the SkTypeface.
        It does not perform kerning or other complex shaping; glyphs are
        positioned based on their default advances.

        @param string   character code points or glyphs drawn
        @param font     text size, typeface, text scale, and so on, used to draw
        @param encoding text encoding used in the text array
        @return         SkTextBlob constructed from one run
    */
    virtual ResourceHandle<IGUIFontTextBlob> createTextBlob(const char* string, const IGUIFont* font, TextEncoding encoding = TextEncoding::kUTF8);

    /** Returns a textblob built from a single run of text with x-positions and a single y value.
        This is equivalent to using SkTextBlobBuilder and calling allocRunPosH().
        Returns nullptr if byteLength is zero.

        @param text        character code points or glyphs drawn (based on encoding)
        @param byteLength  byte length of text array
        @param xpos    array of x-positions, must contain values for all of the character points.
        @param constY  shared y-position for each character point, to be paired with each xpos.
        @param font    SkFont used for this run
        @param encoding specifies the encoding of the text array.
        @return        new textblob or nullptr
     */
    virtual ResourceHandle<IGUIFontTextBlob> createTextBlobPositionH(const void* text, size_t byteLength,
                              std::span<const float> xpos, float constY,
                              const IGUIFont& font,
                              TextEncoding encoding = TextEncoding::kUTF8) = 0;

    /** Returns a textblob built from a single run of text with positions.
        This is equivalent to using SkTextBlobBuilder and calling allocRunPos().
        Returns nullptr if byteLength is zero.

        @param text        character code points or glyphs drawn (based on encoding)
        @param byteLength  byte length of text array
        @param pos     array of positions, must contain values for all of the character points.
        @param font    SkFont used for this run
        @param encoding specifies the encoding of the text array.
        @return        new textblob or nullptr
     */
    virtual ResourceHandle<IGUIFontTextBlob> createTextBlobPosition(const void* text, size_t byteLength,
                                         std::span<const WindowPoint> pos, const IGUIFont* font,
                                         TextEncoding encoding = TextEncoding::kUTF8) = 0;
  
private:
    SkiaFontManager mgr;
};

