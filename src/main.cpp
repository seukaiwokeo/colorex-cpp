#include "colorex.h"
#include <thread>
#include <format>
#include <mutex>
#include <vector>
#include <string>
#include <windows.h>

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

struct AppConfig {
    static constexpr int WINDOW_WIDTH = 400;
    static constexpr int WINDOW_HEIGHT = 330;
    static constexpr std::wstring_view CLASS_NAME = L"ColorexClass";
    static constexpr std::wstring_view WINDOW_TITLE = L"Colorex - Color Picker";
    static constexpr std::wstring_view FONT_NAME = L"Segoe UI";
    static constexpr int FONT_SIZE = 18;
};

struct ClickableText {
    RECT rect;
    std::string text;
    std::wstring label;
};

class ColorexApp {
public:
    ColorexApp() = default;
    ~ColorexApp() {
        shutdown();
    }

    ColorexApp(const ColorexApp&) = delete;
    ColorexApp& operator=(const ColorexApp&) = delete;

    bool initialize(HINSTANCE hInstance, int nCmdShow) {
        WNDCLASS wc{};
        wc.lpfnWndProc = WindowProcStatic;
        wc.hInstance = hInstance;
        wc.lpszClassName = AppConfig::CLASS_NAME.data();
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

        if (!RegisterClass(&wc)) {
            return false;
        }

        const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        const int windowX = (screenWidth - AppConfig::WINDOW_WIDTH) / 2;
        const int windowY = (screenHeight - AppConfig::WINDOW_HEIGHT) / 2;

        m_hwnd = CreateWindowEx(0, AppConfig::CLASS_NAME.data(), AppConfig::WINDOW_TITLE.data(),
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            windowX, windowY, AppConfig::WINDOW_WIDTH, AppConfig::WINDOW_HEIGHT,
            nullptr, nullptr, hInstance, this
        );

        if (!m_hwnd) {
            return false;
        }

        m_clickableAreas.reserve(4);

        m_hFont = CreateFont(
            AppConfig::FONT_SIZE, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            AppConfig::FONT_NAME.data()
        );

        m_hClickableFont = CreateFont(
            AppConfig::FONT_SIZE, 0, 0, 0, FW_NORMAL, FALSE, TRUE, FALSE,
            DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            AppConfig::FONT_NAME.data()
        );

        m_picker.onColorChange([this](const Color& color, const POINT& pos) {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_currentColor = color;
            m_cursorPos = pos;
            updateClickableAreas();
            InvalidateRect(m_hwnd, nullptr, TRUE);
            });

        m_keyThread = std::jthread([this](std::stop_token stoken) {
            keyCheckThread(stoken);
            });

        m_picker.start();

        SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        ShowWindow(m_hwnd, nCmdShow);

        return true;
    }

    int run() {
        MSG msg{};
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return static_cast<int>(msg.wParam);
    }

    void shutdown() {
        m_running = false;
        m_picker.stop();

        if (m_hFont) {
            DeleteObject(m_hFont);
            m_hFont = nullptr;
        }

        if (m_hClickableFont) {
            DeleteObject(m_hClickableFont);
            m_hClickableFont = nullptr;
        }
    }

private:
    void keyCheckThread(std::stop_token stoken) {
        while (!stoken.stop_requested() && m_running) {
            if (GetAsyncKeyState(VK_SPACE) & 0x1) {
                PostMessage(m_hwnd, WM_APP, 1, 0);
            }
            Sleep(10);
        }
    }

    void toggleColorPicker() {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_isActive = !m_isActive;

        if (m_isActive) {
            m_picker.start();
        }
        else {
            m_picker.stop();
        }

        InvalidateRect(m_hwnd, nullptr, TRUE);
    }

    void copyToClipboard(const std::string& text) {
        if (!OpenClipboard(m_hwnd)) return;

        EmptyClipboard();

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hMem) {
            char* pMem = static_cast<char*>(GlobalLock(hMem));
            if (pMem) {
                memcpy(pMem, text.c_str(), text.size() + 1);
                GlobalUnlock(hMem);
                SetClipboardData(CF_TEXT, hMem);
            }
        }

        CloseClipboard();
    }

    void updateClickableAreas() {
        m_clickableAreas.clear();

        constexpr int x = 80;
        constexpr int width = 100;
        constexpr int height = 20;
        constexpr int ySpacing = 30;

        RECT rect = { x, 80, x + width, 80 + height };

        const std::string& hexColor = m_currentColor.toHex();
        m_clickableAreas.emplace_back(rect, hexColor, L"HEX: ");

        rect.top += ySpacing;
        rect.bottom += ySpacing;

        const std::string& rgbColor = m_currentColor.toRGB();
        m_clickableAreas.emplace_back(rect, rgbColor, L"RGB: ");

        rect.top += ySpacing;
        rect.bottom += ySpacing;

        const std::string& hslColor = m_currentColor.toHSL();
        m_clickableAreas.emplace_back(rect, hslColor, L"HSL: ");

        rect.top += ySpacing;
        rect.bottom += ySpacing;

        const std::string& cmykColor = m_currentColor.toCMYK();
        m_clickableAreas.emplace_back(rect, cmykColor, L"CMYK: ");
    }

    void renderColorInfo(HDC hdc) {
        std::lock_guard<std::mutex> lock(m_stateMutex);

        HFONT hOldFont = static_cast<HFONT>(SelectObject(hdc, m_hFont));
        COLORREF oldTextColor = GetTextColor(hdc);

        SetBkMode(hdc, TRANSPARENT);

        std::wstring statusText = m_isActive ? L"Active (Press SPACE key to lock)" : L"Locked (Press SPACE key to unlock)";
        std::wstring posText = std::format(L"Position: X={}, Y={}", m_cursorPos.x, m_cursorPos.y);

        TextOut(hdc, 20, 20, statusText.c_str(), statusText.length());
        TextOut(hdc, 20, 50, posText.c_str(), posText.length());

        RECT checkboxRect = { 20, 200, 40, 220 };
        DrawFrameControl(hdc, &checkboxRect, DFC_BUTTON, m_isTopMost ? DFCS_CHECKED : DFCS_BUTTONCHECK);
        TextOut(hdc, 45, 200, L"Always on top", 14);

        for (const auto& area : m_clickableAreas) {
            TextOut(hdc, 20, area.rect.top, area.label.c_str(), area.label.length());

            SelectObject(hdc, m_hClickableFont);
            SetTextColor(hdc, RGB(0, 0, 255));

            std::wstring wValue(area.text.begin(), area.text.end());
            TextOut(hdc, area.rect.left, area.rect.top, wValue.c_str(), wValue.length());

            SetTextColor(hdc, RGB(0, 0, 0));
            SelectObject(hdc, m_hFont);
        }

        COLORREF colorRef = m_currentColor.toCOLORREF();
        RECT colorRect = { 300, 80, 370, 150 };

        HBRUSH hColorBrush = CreateSolidBrush(colorRef);
        if (hColorBrush) {
            FillRect(hdc, &colorRect, hColorBrush);
            DeleteObject(hColorBrush);
        }
        else {
            HBRUSH hGrayBrush = CreateSolidBrush(RGB(128, 128, 128));
            FillRect(hdc, &colorRect, hGrayBrush);
            DeleteObject(hGrayBrush);
        }

        SetTextColor(hdc, RGB(102, 102, 102));
        TextOut(hdc, 20, 240, L"Click on blue text to copy value", 32);

        SetTextColor(hdc, oldTextColor);
        SelectObject(hdc, hOldFont);
    }

    static LRESULT CALLBACK WindowProcStatic(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        ColorexApp* pApp = nullptr;

        if (uMsg == WM_CREATE) {
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            pApp = static_cast<ColorexApp*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pApp));
        }
        else {
            pApp = reinterpret_cast<ColorexApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (pApp) {
            return pApp->windowProc(hwnd, uMsg, wParam, lParam);
        }

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    LRESULT windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);

                std::lock_guard<std::mutex> lock(m_stateMutex);
                for (const auto& area : m_clickableAreas) {
                    if (PtInRect(&area.rect, pt)) {
                        SetCursor(LoadCursor(nullptr, IDC_HAND));
                        return TRUE;
                    }
                }

                SetCursor(LoadCursor(nullptr, IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_LBUTTONDOWN: {
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);

            if (xPos >= 20 && xPos <= 40 && yPos >= 200 && yPos <= 220) {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_isTopMost = !m_isTopMost;
                SetWindowPos(hwnd, m_isTopMost ? HWND_TOPMOST : HWND_NOTOPMOST,
                    0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }

            std::lock_guard<std::mutex> lock(m_stateMutex);
            for (const auto& area : m_clickableAreas) {
                if (PtInRect(&area.rect, { xPos, yPos })) {
                    copyToClipboard(area.text);

                    return 0;
                }
            }
            break;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == 1001) {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_isTopMost = !m_isTopMost;
                SetWindowPos(hwnd, m_isTopMost ? HWND_TOPMOST : HWND_NOTOPMOST,
                    0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                return 0;
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            renderColorInfo(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_APP:
            if (wParam == 1) {
                toggleColorPicker();
                return 0;
            }
            break;
        }

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

private:
    HWND m_hwnd = nullptr;
    HFONT m_hFont = nullptr;
    HFONT m_hClickableFont = nullptr;
    ColorPicker m_picker;
    std::jthread m_keyThread;
    std::atomic<bool> m_running = true;

    std::mutex m_stateMutex;
    Color m_currentColor{ 0, 0, 0 };
    POINT m_cursorPos{ 0, 0 };
    bool m_isActive = true;
    bool m_isTopMost = true;

    std::vector<ClickableText> m_clickableAreas;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
        return 1;
    }

    ColorexApp app;

    int result = 0;
    if (app.initialize(hInstance, nCmdShow)) {
        result = app.run();
    }

    CoUninitialize();

    return result;
}