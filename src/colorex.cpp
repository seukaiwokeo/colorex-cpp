#include "colorex.h"
#include <format>
#include <algorithm>
#include <cmath>
#include <numbers>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

std::string Color::toHex() const {
    return std::format("#{:02x}{:02x}{:02x}", 
        static_cast<int>(r), static_cast<int>(g), static_cast<int>(b));
}

std::string Color::toRGB() const {
    return std::format("rgb({}, {}, {})", 
        static_cast<int>(r), static_cast<int>(g), static_cast<int>(b));
}

std::string Color::toHSL() const {
    const float rf = r / 255.0f;
    const float gf = g / 255.0f;
    const float bf = b / 255.0f;

    const float maxVal = std::max({rf, gf, bf});
    const float minVal = std::min({rf, gf, bf});
    const float delta = maxVal - minVal;
    const float lightness = (maxVal + minVal) / 2.0f;

    float hue = 0.0f;
    float saturation = 0.0f;

    if (delta > 0.0001f) {
        saturation = lightness > 0.5f 
            ? delta / (2.0f - maxVal - minVal) 
            : delta / (maxVal + minVal);

        if (maxVal == rf) {
            hue = (gf - bf) / delta + (gf < bf ? 6.0f : 0.0f);
        } else if (maxVal == gf) {
            hue = (bf - rf) / delta + 2.0f;
        } else {
            hue = (rf - gf) / delta + 4.0f;
        }
        hue /= 6.0f;
    }

    const int hDeg = static_cast<int>(std::round(hue * 360.0f));
    const int sPct = static_cast<int>(std::round(saturation * 100.0f));
    const int lPct = static_cast<int>(std::round(lightness * 100.0f));

    return std::format("hsl({}, {}%, {}%)", hDeg, sPct, lPct);
}

std::string Color::toCMYK() const {
    if (r == 0 && g == 0 && b == 0) {
        return "cmyk(0%, 0%, 0%, 100%)";
    }

    const float rf = r / 255.0f;
    const float gf = g / 255.0f;
    const float bf = b / 255.0f;

    const float k = 1.0f - std::max({rf, gf, bf});
    
    const auto calcComponent = [k](float value) -> float {
        if (std::abs(k - 1.0f) < 0.0001f) {
            return 0.0f;
        }
        return (1.0f - value - k) / (1.0f - k);
    };
    
    const float c = calcComponent(rf);
    const float m = calcComponent(gf);
    const float y = calcComponent(bf);

    const int cPct = static_cast<int>(std::round(c * 100.0f));
    const int mPct = static_cast<int>(std::round(m * 100.0f));
    const int yPct = static_cast<int>(std::round(y * 100.0f));
    const int kPct = static_cast<int>(std::round(k * 100.0f));

    return std::format("cmyk({}%, {}%, {}%, {}%)", cPct, mPct, yPct, kPct);
}

COLORREF Color::toCOLORREF() const noexcept {
    return RGB(r, g, b);
}

ColorPicker::ColorPicker(int refreshRate)
    : m_refreshRate(refreshRate) 
{
}

ColorPicker::~ColorPicker() {
    stop();
}

void ColorPicker::start() {
    if (m_running) {
        return;
    }

    m_running = true;
    m_thread = CreateThread(nullptr, 0, trackingThread, this, 0, nullptr);
    
    if (m_thread) {
        SetThreadPriority(m_thread, THREAD_PRIORITY_BELOW_NORMAL);
    }
}

void ColorPicker::stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    
    if (m_thread) {
        WaitForSingleObject(m_thread, INFINITE);
        CloseHandle(m_thread);
        m_thread = nullptr;
    }
}

bool ColorPicker::isRunning() const noexcept {
    return m_running;
}

void ColorPicker::onColorChange(ColorChangeCallback callback) {
    m_callback = std::move(callback);
}

Color ColorPicker::getCurrentColor() const noexcept {
    return m_currentColor;
}

POINT ColorPicker::getPosition() const noexcept {
    return m_lastPosition;
}

Color ColorPicker::captureColorAtCursor() {
    POINT cursorPos;
    if (GetCursorPos(&cursorPos)) {
        m_lastPosition = cursorPos;
        
        if (HDC hdcScreen = GetDC(nullptr)) {
            COLORREF colorRef = GetPixel(hdcScreen, cursorPos.x, cursorPos.y);
            ReleaseDC(nullptr, hdcScreen);
            
            if (colorRef != CLR_INVALID) {
                return Color::fromCOLORREF(colorRef);
            }
        }
    }
    
    return m_currentColor;
}

DWORD WINAPI ColorPicker::trackingThread(LPVOID param) {
    auto* picker = static_cast<ColorPicker*>(param);
    
    Color lastColor{0, 0, 0};
    POINT lastPos{-1, -1};
    
    while (picker->m_running) {
        Color newColor = picker->captureColorAtCursor();
        POINT newPos = picker->m_lastPosition;
        
        picker->m_currentColor = newColor;
        
        if (picker->m_callback && (newColor != lastColor || 
            newPos.x != lastPos.x || newPos.y != lastPos.y)) {
            
            (*picker->m_callback)(newColor, newPos);
            
            lastColor = newColor;
            lastPos = newPos;
        }
        
        Sleep(picker->m_refreshRate);
    }
    
    return 0;
}