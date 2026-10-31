// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: gtkWindowUtilities.h
 * Purpose: Adds GTK 4 and 3 support for window utilities
 */
#pragma once
#include "nWindow/windowUtilities.h"

class gtkWindowUtilities
{
    static const char* getIconName(MessageBoxIcon icon);
public:
    static bool createMessageBox(SysHandle handle, const char* title, const char* message, MessageBoxIcon icons);
    static bool createMessageBox(SysHandle handle, const wchar_t* title, const wchar_t* message, MessageBoxIcon icons);
    [[nodiscard]] static constexpr bool supported() noexcept
    {
#if defined(BORA_HAS_GTK4) || defined(BORA_HAS_GTK3)
        return true;
#else
        return false;
#endif
    }
};
