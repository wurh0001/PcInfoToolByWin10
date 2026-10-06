#include "ui/controls/CustomContextMenu.h"
#include "ui/platform/DpiHelper.h"
#include <algorithm>

CustomContextMenu::CustomContextMenu() {}

CustomContextMenu::~CustomContextMenu() {
    if (m_hMenuWnd) {
        DestroyWindow(m_hMenuWnd);
        m_hMenuWnd = nullptr;
    }
    if (m_hSubmenuWnd) {
        DestroyWindow(m_hSubmenuWnd);
        m_hSubmenuWnd = nullptr;
    }
}

void CustomContextMenu::SetTheme(Theme::ThemePreset preset) {
    m_preset = preset;
}

void CustomContextMenu::AddItem(int id, const std::wstring& text, bool isChecked) {
    MenuItem it;
    it.id = id;
    it.text = text;
    it.isChecked = isChecked;
    it.isSeparator = false;
    it.isSubmenu = false;
    m_items.push_back(it);
}

void CustomContextMenu::AddSeparator() {
    MenuItem it;
    it.isSeparator = true;
    m_items.push_back(it);
}

void CustomContextMenu::AddSubMenu(const std::wstring& text, const std::vector<MenuItem>& subItems) {
    MenuItem it;
    it.text = text;
    it.isSubmenu = true;
    it.isSeparator = false;
    it.subItems = subItems;
    m_items.push_back(it);
}

void CustomContextMenu::MeasureMenu(const std::vector<MenuItem>& items, int& outWidth, int& outHeight) {
    float maxTextW = 140.0f;
    float totalH = 14.0f; // top/bottom padding: 7px each

    for (const auto& item : items) {
        if (item.isSeparator) {
            totalH += 9.0f;
        } else {
            totalH += 30.0f;
            float approxW = static_cast<float>(item.text.length()) * 13.5f + 48.0f;
            if (item.isSubmenu) approxW += 20.0f;
            if (approxW > maxTextW) maxTextW = approxW;
        }
    }

    outWidth = DpiHelper::DipToPx(maxTextW, m_dpiScale);
    outHeight = DpiHelper::DipToPx(totalH, m_dpiScale);
}

void CustomContextMenu::RenderMenu(HWND hwnd, const std::vector<MenuItem>& items, int hoveredIndex) {
    LayeredD2DRenderer& r = (hwnd == m_hSubmenuWnd) ? m_subRenderer : m_renderer;
    auto rt = r.BeginDraw();
    if (!rt) return;

    RECT rc;
    GetClientRect(hwnd, &rc);
    float W = static_cast<float>(rc.right - rc.left) / m_dpiScale;
    float H = static_cast<float>(rc.bottom - rc.top) / m_dpiScale;

    rt->SetTransform(D2D1::Matrix3x2F::Scale(m_dpiScale, m_dpiScale));

    auto pal = Theme::GetPresetPalette(m_preset);

    // 1. Menu background rounded rectangle
    D2D1_ROUNDED_RECT menuRc = D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, W - 0.5f, H - 0.5f), 10.0f, 10.0f);

    ID2D1SolidColorBrush* pBgBrush = nullptr;
    ID2D1SolidColorBrush* pBorderBrush = nullptr;
    ID2D1SolidColorBrush* pSepBrush = nullptr;
    ID2D1SolidColorBrush* pHoverBrush = nullptr;
    ID2D1SolidColorBrush* pTextBrush = nullptr;
    ID2D1SolidColorBrush* pCheckBrush = nullptr;

    rt->CreateSolidColorBrush(pal.WindowBg, &pBgBrush);
    rt->CreateSolidColorBrush(pal.WindowBorder, &pBorderBrush);
    rt->CreateSolidColorBrush(pal.DividerLine, &pSepBrush);

    // Hover background color
    D2D1_COLOR_F hoverCol = pal.isLight ?
        D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.08f) :
        D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f);
    rt->CreateSolidColorBrush(hoverCol, &pHoverBrush);
    rt->CreateSolidColorBrush(pal.TextPrimary, &pTextBrush);
    rt->CreateSolidColorBrush(pal.NetDownGreen, &pCheckBrush);

    if (pBgBrush) rt->FillRoundedRectangle(menuRc, pBgBrush);

    // Specular Rim Gradient Highlight for translucent frosted glass
    if (pal.isTranslucent) {
        ID2D1GradientStopCollection* pStops = nullptr;
        D2D1_GRADIENT_STOP stops[3];
        stops[0].color = pal.WindowBorder;
        stops[0].position = 0.0f;
        stops[1].color = D2D1::ColorF(pal.WindowBorder.r, pal.WindowBorder.g, pal.WindowBorder.b, pal.WindowBorder.a * 0.40f);
        stops[1].position = 0.50f;
        stops[2].color = D2D1::ColorF(pal.WindowBorder.r, pal.WindowBorder.g, pal.WindowBorder.b, pal.WindowBorder.a * 0.20f);
        stops[2].position = 1.0f;

        rt->CreateGradientStopCollection(stops, 3, &pStops);
        if (pStops) {
            ID2D1LinearGradientBrush* pRim = nullptr;
            rt->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(0.0f, 0.5f), D2D1::Point2F(0.0f, H - 0.5f)),
                pStops, &pRim
            );
            if (pRim) {
                rt->DrawRoundedRectangle(menuRc, pRim, 1.2f);
                pRim->Release();
            }
            pStops->Release();
        }
    } else {
        if (pBorderBrush) rt->DrawRoundedRectangle(menuRc, pBorderBrush, 1.0f);
    }

    float curY = 7.0f;
    for (size_t i = 0; i < items.size(); ++i) {
        const auto& it = items[i];
        if (it.isSeparator) {
            if (pSepBrush) {
                rt->DrawLine(D2D1::Point2F(10.0f, curY + 4.0f), D2D1::Point2F(W - 10.0f, curY + 4.0f), pSepBrush, 1.0f);
            }
            curY += 9.0f;
        } else {
            float itemH = 30.0f;
            D2D1_ROUNDED_RECT itemRc = D2D1::RoundedRect(D2D1::RectF(5.0f, curY, W - 5.0f, curY + itemH), 6.0f, 6.0f);

            // Highlight hover
            if (static_cast<int>(i) == hoveredIndex && pHoverBrush) {
                rt->FillRoundedRectangle(itemRc, pHoverBrush);
            }

            // Draw Checkmark
            if (it.isChecked) {
                if (pCheckBrush) {
                    // Checkmark tick symbol "✔"
                    if (r.GetFormatValue()) {
                        rt->DrawTextW(L"✔", 1, r.GetFormatValue(),
                            D2D1::RectF(9.0f, curY + 5.0f, 25.0f, curY + 25.0f), pCheckBrush);
                    }
                }
            }

            // Text
            float textLeft = 27.0f;
            float textRight = it.isSubmenu ? (W - 22.0f) : (W - 10.0f);
            if (pTextBrush && r.GetFormatLabel()) {
                rt->DrawTextW(it.text.c_str(), static_cast<UINT32>(it.text.length()), r.GetFormatLabel(),
                    D2D1::RectF(textLeft, curY + 5.5f, textRight, curY + 26.5f), pTextBrush);
            }

            // Draw arrow if submenu
            if (it.isSubmenu && pTextBrush) {
                float arrowX = W - 14.0f;
                float arrowY = curY + itemH / 2.0f;
                rt->DrawLine(D2D1::Point2F(arrowX - 4.0f, arrowY - 4.0f), D2D1::Point2F(arrowX, arrowY), pTextBrush, 1.5f);
                rt->DrawLine(D2D1::Point2F(arrowX, arrowY), D2D1::Point2F(arrowX - 4.0f, arrowY + 4.0f), pTextBrush, 1.5f);
            }

            curY += itemH;
        }
    }

    if (pBgBrush) pBgBrush->Release();
    if (pBorderBrush) pBorderBrush->Release();
    if (pSepBrush) pSepBrush->Release();
    if (pHoverBrush) pHoverBrush->Release();
    if (pTextBrush) pTextBrush->Release();
    if (pCheckBrush) pCheckBrush->Release();

    r.EndDraw();
}

LRESULT CALLBACK CustomContextMenu::MenuWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CustomContextMenu* pThis = reinterpret_cast<CustomContextMenu*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!pThis) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_MOUSEMOVE: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        float dipY = static_cast<float>(pt.y) / pThis->m_dpiScale;

        int newHover = -1;
        float curY = 7.0f;
        for (size_t i = 0; i < pThis->m_items.size(); ++i) {
            if (pThis->m_items[i].isSeparator) {
                curY += 9.0f;
            } else {
                if (dipY >= curY && dipY <= curY + 30.0f) {
                    newHover = static_cast<int>(i);
                    break;
                }
                curY += 30.0f;
            }
        }

        if (newHover != pThis->m_hoveredIndex) {
            pThis->m_hoveredIndex = newHover;
            pThis->RenderMenu(hwnd, pThis->m_items, pThis->m_hoveredIndex);

            // Handle submenu show/hide
            if (newHover >= 0 && pThis->m_items[newHover].isSubmenu) {
                if (pThis->m_openSubmenuIndex != newHover) {
                    pThis->m_openSubmenuIndex = newHover;

                    // Calculate submenu pos
                    RECT mRc;
                    GetWindowRect(hwnd, &mRc);
                    int subW = 0, subH = 0;
                    pThis->MeasureMenu(pThis->m_items[newHover].subItems, subW, subH);

                    // Vertical offset corresponding to item
                    float itemTopDip = 7.0f;
                    for (int k = 0; k < newHover; ++k) {
                        itemTopDip += pThis->m_items[k].isSeparator ? 9.0f : 30.0f;
                    }
                    int itemTopPx = mRc.top + DpiHelper::DipToPx(itemTopDip, pThis->m_dpiScale);

                    int subX = mRc.right - DpiHelper::DipToPx(4.0f, pThis->m_dpiScale);
                    int subY = itemTopPx - DpiHelper::DipToPx(4.0f, pThis->m_dpiScale);

                    // Clamp to screen bounds
                    int screenW = GetSystemMetrics(SM_CXSCREEN);
                    int screenH = GetSystemMetrics(SM_CYSCREEN);
                    if (subX + subW > screenW - 8) {
                        subX = mRc.left - subW + DpiHelper::DipToPx(4.0f, pThis->m_dpiScale);
                    }
                    if (subY + subH > screenH - 8) {
                        subY = screenH - subH - 8;
                    }

                    pThis->m_subRenderer.Resize(subW, subH);
                    SetWindowPos(pThis->m_hSubmenuWnd, HWND_TOPMOST, subX, subY, subW, subH, SWP_SHOWWINDOW | SWP_NOACTIVATE);
                    pThis->m_subHoveredIndex = -1;
                    pThis->RenderMenu(pThis->m_hSubmenuWnd, pThis->m_items[newHover].subItems, -1);
                }
            } else if (newHover >= 0 && !pThis->m_items[newHover].isSubmenu) {
                if (pThis->m_hSubmenuWnd && IsWindowVisible(pThis->m_hSubmenuWnd)) {
                    ShowWindow(pThis->m_hSubmenuWnd, SW_HIDE);
                    pThis->m_openSubmenuIndex = -1;
                }
            }
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (pThis->m_hoveredIndex >= 0 && pThis->m_hoveredIndex < (int)pThis->m_items.size()) {
            const auto& it = pThis->m_items[pThis->m_hoveredIndex];
            if (!it.isSeparator && !it.isSubmenu) {
                pThis->m_selectedCmd = it.id;
                pThis->m_looping = false;
            }
        }
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK CustomContextMenu::SubmenuWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CustomContextMenu* pThis = reinterpret_cast<CustomContextMenu*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!pThis) return DefWindowProc(hwnd, msg, wParam, lParam);

    if (pThis->m_openSubmenuIndex < 0 || pThis->m_openSubmenuIndex >= (int)pThis->m_items.size()) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    const auto& subItems = pThis->m_items[pThis->m_openSubmenuIndex].subItems;

    switch (msg) {
    case WM_MOUSEMOVE: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        float dipY = static_cast<float>(pt.y) / pThis->m_dpiScale;

        int newHover = -1;
        float curY = 7.0f;
        for (size_t i = 0; i < subItems.size(); ++i) {
            if (subItems[i].isSeparator) {
                curY += 9.0f;
            } else {
                if (dipY >= curY && dipY <= curY + 30.0f) {
                    newHover = static_cast<int>(i);
                    break;
                }
                curY += 30.0f;
            }
        }

        if (newHover != pThis->m_subHoveredIndex) {
            pThis->m_subHoveredIndex = newHover;
            pThis->RenderMenu(hwnd, subItems, pThis->m_subHoveredIndex);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (pThis->m_subHoveredIndex >= 0 && pThis->m_subHoveredIndex < (int)subItems.size()) {
            const auto& it = subItems[pThis->m_subHoveredIndex];
            if (!it.isSeparator && !it.isSubmenu) {
                pThis->m_selectedCmd = it.id;
                pThis->m_looping = false;
            }
        }
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int CustomContextMenu::Track(HWND hParent, int screenX, int screenY) {
    m_dpiScale = DpiHelper::GetScaleForWindow(hParent);

    // Register Window Classes if needed
    static bool registered = false;
    if (!registered) {
        WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
        wc.lpfnWndProc = CustomContextMenu::MenuWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = L"PcInfoTool_CustomMenuClass";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassEx(&wc);

        WNDCLASSEX wcs = { sizeof(WNDCLASSEX) };
        wcs.lpfnWndProc = CustomContextMenu::SubmenuWndProc;
        wcs.hInstance = GetModuleHandle(NULL);
        wcs.lpszClassName = L"PcInfoTool_CustomSubmenuClass";
        wcs.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassEx(&wcs);

        registered = true;
    }

    int menuW = 0, menuH = 0;
    MeasureMenu(m_items, menuW, menuH);

    // Clamp coordinates to monitor screen
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    if (screenX + menuW > screenW - 8) {
        screenX = screenW - menuW - 8;
    }
    if (screenY + menuH > screenH - 8) {
        screenY = screenY - menuH;
    }

    // Create main menu window
    m_hMenuWnd = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED,
        L"PcInfoTool_CustomMenuClass", L"",
        WS_POPUP,
        screenX, screenY, menuW, menuH,
        hParent, NULL, GetModuleHandle(NULL), NULL
    );
    SetWindowLongPtr(m_hMenuWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    m_renderer.Initialize(m_hMenuWnd, menuW, menuH);

    // Create submenu window
    m_hSubmenuWnd = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED,
        L"PcInfoTool_CustomSubmenuClass", L"",
        WS_POPUP,
        0, 0, 10, 10,
        m_hMenuWnd, NULL, GetModuleHandle(NULL), NULL
    );
    SetWindowLongPtr(m_hSubmenuWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    m_subRenderer.Initialize(m_hSubmenuWnd, 10, 10);

    // Initial render & show
    m_hoveredIndex = -1;
    m_selectedCmd = 0;
    m_openSubmenuIndex = -1;
    RenderMenu(m_hMenuWnd, m_items, -1);
    ShowWindow(m_hMenuWnd, SW_SHOW);
    SetForegroundWindow(m_hMenuWnd);

    // Modal message loop
    m_looping = true;
    MSG msg;
    while (m_looping) {
        // Check mouse click outside both menus to dismiss
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000 || GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
            POINT pt;
            GetCursorPos(&pt);
            RECT mRc, sRc = {};
            GetWindowRect(m_hMenuWnd, &mRc);
            bool overSub = false;
            if (m_hSubmenuWnd && IsWindowVisible(m_hSubmenuWnd)) {
                GetWindowRect(m_hSubmenuWnd, &sRc);
                overSub = PtInRect(&sRc, pt);
            }
            if (!PtInRect(&mRc, pt) && !overSub) {
                m_looping = false;
                break;
            }
        }
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            m_looping = false;
            break;
        }

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_looping = false;
                PostQuitMessage((int)msg.wParam);
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(10);
    }

    if (m_hSubmenuWnd) {
        DestroyWindow(m_hSubmenuWnd);
        m_hSubmenuWnd = nullptr;
    }
    if (m_hMenuWnd) {
        DestroyWindow(m_hMenuWnd);
        m_hMenuWnd = nullptr;
    }

    return m_selectedCmd;
}
