#pragma once
#include <string>
#include <string_view>
#include <windows.h>
#include <functional>
#include <atomic>
#include <optional>
#include <array>
#include <span>
#include <concepts>

struct Color {
    uint8_t r{0}, g{0}, b{0};

    std::string toHex() const;
    std::string toRGB() const;
    std::string toHSL() const;
    std::string toCMYK() const;
    COLORREF toCOLORREF() const noexcept;
    
    bool operator==(const Color& other) const noexcept = default;
    
    static Color fromCOLORREF(COLORREF colorRef) noexcept {
        return {
            static_cast<uint8_t>(GetRValue(colorRef)), 
            static_cast<uint8_t>(GetGValue(colorRef)), 
            static_cast<uint8_t>(GetBValue(colorRef))
        };
    }
};

class ColorPicker {
public:
    using ColorChangeCallback = std::function<void(const Color&, const POINT&)>;

    explicit ColorPicker(int refreshRate = 10);
    ~ColorPicker();

    ColorPicker(const ColorPicker&) = delete;
    ColorPicker& operator=(const ColorPicker&) = delete;

    void start();
    void stop();
    bool isRunning() const noexcept;
    void onColorChange(ColorChangeCallback callback);
    Color getCurrentColor() const noexcept;
    POINT getPosition() const noexcept;

private:
    Color captureColorAtCursor();
    static DWORD WINAPI trackingThread(LPVOID param);

private:
    std::atomic<bool> m_running{false};
    int m_refreshRate;
    POINT m_lastPosition{0, 0};
    Color m_currentColor{0, 0, 0};
    std::optional<ColorChangeCallback> m_callback;
    HANDLE m_thread{nullptr};
};