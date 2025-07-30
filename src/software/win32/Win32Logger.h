#pragma once
#include <string>
#include <iostream>
#include <windows.h>
#include <format>  // For C++20 formatting, can switch to snprintf for pre-C++20
#include <sstream>
#include <iomanip>  // For std::fixed, std::setprecision

enum ConsoleColor {
    Default = -1,
    Black = 30,       // Black text
    Red = 31,         // Red text
    Green = 32,       // Green text
    Yellow = 33,      // Yellow text
    Blue = 34,        // Blue text
    Magenta = 35,     // Magenta text
    Cyan = 36,        // Cyan text
    White = 37,       // White text
    BrightBlack = 90,   // Gray text
    BrightRed = 91,     // Bright Red text
    BrightGreen = 92,   // Bright Green text
    BrightYellow = 93,  // Bright Yellow text
    BrightBlue = 94,    // Bright Blue text
    BrightMagenta = 95, // Bright Magenta text
    BrightCyan = 96,    // Bright Cyan text
    BrightWhite = 97    // Bright White text
};


    struct WDediErrorD {
        PCWSTR Icon = NULL;
        std::wstring Title;
        std::wstring Header;
        std::wstring Description = L"";
        bool enAdvancedInfo;
        std::wstring AdvancedInfoC = L"";
        std::wstring AdvancedInfoTitle = L"";
        UINT width = 0;
    };




    class OPLogger {
    public:

        // Basic log functions (<< support)
        bool CustomLog(std::string output, int color = -1);
        bool Error(std::string output, int critical);
        bool WError(std::wstring output, int critical);
        bool DediError(WDediErrorD data);
        void clear();
      
        // Overloaded log function with variadic arguments for %s support
        template <typename... Args>
        bool CustomLog(const std::string& formatStr, int color, Args&&... args) {
            std::string formatted = std::vformat(formatStr, std::make_format_args(std::forward<Args>(args)...));
            return CustomLog(formatted, color);
        }

        template <typename... Args>
        bool Error(const std::string& formatStr, int critical, Args&&... args) {
            std::string formatted = std::vformat(formatStr, std::make_format_args(std::forward<Args>(args)...));
            return Error(formatted, critical);
        }

        template <typename... Args>
        bool WError(const std::wstring& formatStr, int critical, Args&&... args) {
            std::wstring formatted = std::vformat(formatStr, std::make_wformat_args(std::forward<Args>(args)...));
            return WError(formatted, critical);
        }

    public:
        static OPLogger get();


        /// <summary>
            /// INFO is the only operator available
            /// As it's the simplest, you can use for simple logs. 
            /// Use OPLOGGER's dynanic function.
            /// </summary>
        class INFO {
        public:

            INFO& operator<<(const std::string& message) {
                buffer_ << message;
                return *this;
            }

            INFO& operator<<(int value) {
                buffer_ << value;
                return *this;
            }

            // Overload << operator for float
            INFO& operator<<(float value) {
                buffer_ << std::fixed << std::setprecision(2) << value;  // Format float with precision 2
                return *this;
            }

            // Overload << operator for char
            INFO& operator<<(char value) {
                buffer_ << value;
                return *this;
            }

            // Overload << operator for const char* (C-style strings)
            INFO& operator<<(const char* value) {
                buffer_ << value;
                return *this;
            }

            // Overload << operator for other types (optional, useful for double, long, etc.)
            template<typename T>
            INFO& operator<<(const T& value) {
                buffer_ << value;
                return *this;
            }


            INFO& operator<<(std::ostream& (*fp)(std::ostream&)) {
                HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                SetConsoleTextAttribute(hConsole, 15);
                std::cout << buffer_.str() << std::endl;
                buffer_.str("");  // Clear the buffer after logging
                return *this;
            }

        private:
            std::ostringstream buffer_;
        };
    };

