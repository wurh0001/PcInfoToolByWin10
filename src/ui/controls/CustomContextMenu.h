/*
 * 模块说明：Lite 使用的自绘菜单数据与接口
 * MenuItem 同时表达普通项、分隔线、勾选项和子菜单；Track 返回选择的命令。
 * 主菜单和子菜单各有窗口与渲染器，悬停索引与命令 ID 的含义不同。
 */
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include "ui/theme/Theme.h"
#include "ui/rendering/LayeredD2DRenderer.h"

class CustomContextMenu {
public:
    struct MenuItem {
        int id = 0;
        std::wstring text;
        bool isSeparator = false;
        bool isChecked = false;
        bool isSubmenu = false;
        std::vector<MenuItem> subItems;
    };

    CustomContextMenu();
    ~CustomContextMenu();

    /**
     * 设置菜单主题预设，后续绘制读取相应颜色。
     */
    void SetTheme(Theme::ThemePreset preset);
    /**
     * 加入普通菜单项，命令 ID 与显示文字分别保存。
     */
    void AddItem(int id, const std::wstring& text, bool isChecked = false);
    /**
     * 加入分组分隔线，不作为可执行命令。
     */
    void AddSeparator();
    /**
     * 加入包含子项的菜单入口，子项列表由本菜单保存。
     */
    void AddSubMenu(const std::wstring& text, const std::vector<MenuItem>& subItems);

    // Track popup menu at screen position, returns chosen cmd id (0 if canceled)
    /**
     * 定位并显示菜单，运行局部消息循环直到选择或取消；返回命令 ID，0 表示取消。
     */
    int Track(HWND hParent, int screenX, int screenY);

private:
    /**
     * 处理主菜单消息，更新悬停/选择并协调子菜单。
     * @param hwnd Win32 窗口句柄，不是 C++ 对象地址。
     * @param msg 窗口消息编号，决定后续分支。
     * @param wParam 消息附加参数，含义随 msg 改变。
     * @param lParam 消息附加参数，鼠标坐标等需按消息格式解码。
     */
    static LRESULT CALLBACK MenuWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    /**
     * 处理子菜单鼠标消息，将选中的命令交回菜单对象。
     * @param hwnd Win32 窗口句柄，不是 C++ 对象地址。
     * @param msg 窗口消息编号，决定后续分支。
     * @param wParam 消息附加参数，含义随 msg 改变。
     * @param lParam 消息附加参数，鼠标坐标等需按消息格式解码。
     */
    static LRESULT CALLBACK SubmenuWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * 根据项目列表和悬停索引绘制指定菜单窗口。
     * @param hwnd Win32 窗口句柄，不是 C++ 对象地址。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void RenderMenu(HWND hwnd, const std::vector<MenuItem>& items, int hoveredIndex);
    /**
     * 计算菜单宽高，输出尺寸供创建和定位窗口使用。
     */
    void MeasureMenu(const std::vector<MenuItem>& items, int& outWidth, int& outHeight);

    Theme::ThemePreset m_preset = Theme::ThemePreset::TranslucentGlass;
    std::vector<MenuItem> m_items;

    HWND m_hMenuWnd = nullptr;
    HWND m_hSubmenuWnd = nullptr;
    LayeredD2DRenderer m_renderer;
    LayeredD2DRenderer m_subRenderer;

    int m_hoveredIndex = -1;
    int m_selectedCmd = 0;
    bool m_looping = false;
    float m_dpiScale = 1.0f;

    // Submenu state
    int m_openSubmenuIndex = -1;
    int m_subHoveredIndex = -1;
    RECT m_submenuRect = {};
};
