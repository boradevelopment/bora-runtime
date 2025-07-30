#include "Win32Logger.h"
#if WIN32
#include <Windows.h>
#include <cstdlib>

#if defined _WIN64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#pragma comment (lib, "Shlwapi.lib")
#pragma comment (lib, "Comctl32.lib")

#include <commctrl.h>


void setConsoleColor(int textColor, int bgColor = ConsoleColor::Black) {
    std::cout << "\033[" << textColor << ";" << (bgColor + 10) << "m";
}

// Function to reset console color
void resetConsoleColor() {
    std::cout << "\033[0m";
}

    bool Win32Logger::CustomLog(std::string output, int color) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (color == -1) color = 0;
        setConsoleColor(color);
        std::cout << output << std::endl;
        resetConsoleColor();
        return true;
    }

    bool Win32Logger::WError(std::wstring output, int critical) {
        setConsoleColor(31);
        if (critical == 0) {
            std::wcout << "[ERROR]: " << output << std::endl;
        }
        else {
            std::wcout << "[CRITICAL ERROR]: " << output << std::endl;
#if isRetail == 0
            MessageBoxW(NULL, output.c_str(), L"Critical Error!", MB_ICONERROR | MB_OK);
#else
            MessageBox(NULL, output.c_str(), L"This game can't continue!", MB_ICONERROR | MB_OK);
#endif
            exit(1);
        }
        resetConsoleColor();
        return true;
    }

    bool Win32Logger::DediError(WDediErrorD data) {
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            setConsoleColor(31);
            std::wcout << "[CRITICAL ERROR - DEDI]: " << data.Description << std::endl;
            resetConsoleColor();

            TASKDIALOGCONFIG config = { 0 };
            config.pszWindowTitle = data.Title.c_str();
            config.dwFlags = TDF_ENABLE_HYPERLINKS;
            config.pszMainIcon = data.Icon;
            config.pszMainInstruction = data.Header.c_str();
            config.pszContent = data.Description.c_str();
            if (data.enAdvancedInfo) {
                config.pszExpandedInformation = data.AdvancedInfoC.c_str();
                config.pszExpandedControlText = data.AdvancedInfoTitle.c_str();
            }
            config.cxWidth = data.width;
            config.cbSize = sizeof(config);
            TaskDialogIndirect(&config, NULL, NULL, NULL);
            exit(1);
            return true;
        
    }

    bool Win32Logger::Error(std::string output, int critical) {
        setConsoleColor(31);
        if (critical == 0) {
            std::cout << "[ERROR]: " << output.c_str() << std::endl;
        }
        else {
            std::cout << "[CRITICAL ERROR]: " << output.c_str() << std::endl;
            resetConsoleColor();
            std::wstring wideOutPut = std::wstring(output.begin(), output.end());
#if isRetail == 0
            MessageBoxW(NULL, wideOutPut.c_str(), L"Critical Error!", MB_ICONERROR | MB_OK);
#else
            MessageBox(NULL, wideOutPut.c_str(), L"This game can't continue!", MB_ICONERROR | MB_OK);
#endif
            exit(1);
        }
        resetConsoleColor();
        return true;
    }

    void Win32Logger::clear() {
        system("cls");
    }

    Win32Logger Win32Logger::get() {
        static Win32Logger OPL;
        return OPL;
    }
#endif