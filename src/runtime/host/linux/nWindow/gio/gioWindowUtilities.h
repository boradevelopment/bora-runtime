// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: gioWindowUtilities.h
 * Purpose: Adds GIO support for window utilities
 */
#pragma once
#include "nWindow/windowUtilities.h"

class gioWindowUtilities
{
    static const char* getIconName(MessageBoxIcon icon);
public:
    static bool createMessageBox(SysHandle handle, const char* title, const char* message, MessageBoxIcon icons);
    static bool createMessageBox(SysHandle handle, const wchar_t* title, const wchar_t* message, MessageBoxIcon icons);
    [[nodiscard]] static constexpr bool supported() noexcept
    {
#ifdef BORA_HAS_GIO
        return true;
#else
        return false;
#endif
    }
};
