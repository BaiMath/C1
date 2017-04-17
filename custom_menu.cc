/**
* \file custom_menu.cc
* \date 2016/05/17 16:39
* \author bao
* \Copyright (C) Maxthon
*/

#include "maxthon/renderer/trident/custom_menu.h"

#include <algorithm>
namespace Gdiplus
{
    using std::min;
    using std::max;
};
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

static ULONG_PTR g_gdiplus_token = 0;
static Gdiplus::GdiplusStartupInput g_gdiplus_startup_input;

/*  menu layout
-------------+-------------------+-----------
    menu left|main part          |menu right
    icon     |text shortcut arrow|
-------------+-------------------+-----------
*/
static const std::wstring kOwnerMenuPropName = L"MxCustomMenu";
static float kDPI = 1.0f;
#define DPI_FLOAT(l)    ((float)l * kDPI)
#define DPI_INT_W(l)    ((int)((float)l * (kDPI * 0.9)))
#define DPI_INT(l)      ((int)((float)l * kDPI))
static int kMenuItemWidth, kMenuItemHeight, kItemIconLeft, kItemIconTop,
    kItemIconWidth, kItemIconHeight, kMenuLeftWidth, kMenuLeftHeight,
    kMenuRightWidth, kMenuRightHeight, kSeparatorHeight, kBorderHeight;
static int kArrowRightSpace, kArrowWidth, kArrowHeight;
static int kCheckWidth, kCheckHeight, kCheckPenSize;

// colors and brushes, fonts
static const Gdiplus::Color kColorMenuBorder(186, 186, 186);
static const Gdiplus::Color kColorNormalText(51, 51, 51); // #333333
static const Gdiplus::Color kColorSelectedText = kColorNormalText; // #333333
static const Gdiplus::Color kColorShortCutText(153, 153, 153); // #999999
static const Gdiplus::Color kColorDisabledText(161, 161, 161);
static const Gdiplus::Color kColorSeparatorBack(228, 228, 228); // #e4e4e4
static const Gdiplus::Color kColorSelectedBack(230, 244, 255); // #e6f4ff
static const Gdiplus::Color kColorMenuBar(255, 255, 255);
static const Gdiplus::Color kColorBorder(213, 233, 242);
static const Gdiplus::Color kColorNormalBack(255, 255, 255);
static const Gdiplus::Color kColorRedText(255, 0, 0);
static const Gdiplus::Color kColorNormalArrow(122, 122, 122);
static const Gdiplus::Color kColorNormalCheck(78, 78, 78);

static const std::wstring kDefaultFontFamilyList[] =
    { L"Microsoft YaHei", L"宋体", L"Arial" };
static std::wstring kDefaultFontFamily = L"Arail";
static const float kDefaultFontSize = 9.0f;
static float kCheckEllipseRadis;

// MxCustomMenu
std::map<HWND, WNDPROC> MxCustomMenu::hook_map_;

MxCustomMenu::MxCustomMenu() : def_wnd_proc_(NULL)
    , owner_wnd_(NULL)
    , item_data_(NULL)
{
}

MxCustomMenu::~MxCustomMenu()
{
    UnHookOwnerWnd();
    if (item_data_) {
        ReleaseItemData(item_data_);
        item_data_ = NULL;
    }
}

void MxCustomMenu::ReleaseItemData(MenuItemData* item_data)
{
    if (!item_data)
        return;

    std::vector<MenuItemData*>::iterator itr = item_data->sub_datas.begin();
    for (; itr != item_data->sub_datas.end(); ++itr)
    {
        MenuItemData* sub_string = *itr;
        ReleaseItemData(sub_string);
    }
    delete item_data;
}

BOOL IsFontFamlilyAvaliable(const std::wstring& font_family)
{
    Gdiplus::FontFamily family(font_family.c_str());
    return family.IsAvailable();
}

void TryInit()
{
    if (g_gdiplus_token)
        return;

    Gdiplus::GdiplusStartup(&g_gdiplus_token,
        &g_gdiplus_startup_input, NULL);

    HINSTANCE user32 = LoadLibrary(L"user32.dll");
    if (user32) {
        typedef BOOL(WINAPI* LPIsProcessDPIAware)(void);
        LPIsProcessDPIAware lp_IsProcessDPIAware =
            (LPIsProcessDPIAware)GetProcAddress(user32, "IsProcessDPIAware");
        if (lp_IsProcessDPIAware) {
            if (lp_IsProcessDPIAware()) {
                HDC hdc = ::GetDC(NULL);
                kDPI = (float)GetDeviceCaps(hdc, LOGPIXELSX) / (float)96;
                ::ReleaseDC(NULL, hdc);
                if (kDPI >= 1.5f)
                    kDPI = 1.5f;
                else if (kDPI >= 1.25f)
                    kDPI = 1.25f;
                else
                    kDPI = 1.0f;
            }
        }
        FreeLibrary(user32);
    }
    kMenuItemWidth = DPI_INT_W(225);
    kMenuItemHeight = DPI_INT(26);
    kItemIconLeft = DPI_INT(10);
    kItemIconTop = DPI_INT(5);
    kItemIconWidth = DPI_INT(16);
    kItemIconHeight = DPI_INT(16);
    kMenuLeftWidth = DPI_INT_W(32);
    kMenuLeftHeight = kMenuItemHeight;
    kMenuRightWidth = DPI_INT_W(15);
    kMenuRightHeight = kMenuItemHeight;
    kSeparatorHeight = DPI_INT(10);
    kBorderHeight = DPI_INT(5);

    kArrowRightSpace = DPI_INT(8);
    kArrowWidth = DPI_INT(4);
    kArrowHeight = DPI_INT(8);

    kCheckWidth = DPI_INT(10);
    kCheckHeight = DPI_INT(10);
    kCheckPenSize = DPI_FLOAT(2.0f);
    kCheckEllipseRadis = DPI_FLOAT(3.5f);

    // check avaliable font
    size_t font_size =
        sizeof(kDefaultFontFamilyList) / sizeof(kDefaultFontFamilyList[0]);
    for (size_t i = 0; i < font_size; ++i) {
        if (IsFontFamlilyAvaliable(kDefaultFontFamilyList[i])) {
            kDefaultFontFamily = kDefaultFontFamilyList[i];
            break;
        }
    }
}

BOOL MxCustomMenu::TrackPopupMenu(UINT flags, int x, int y, HWND hwnd,
    LPCRECT rect/* = NULL*/)
{
    TryInit();
    if (!item_data_)
        item_data_ = new MenuItemData;
    EnableOwnerDraw(m_hMenu, item_data_);
    HookOwnerWnd(hwnd);
    return CMenu::TrackPopupMenu(flags, x, y, hwnd, rect);
}

void MxCustomMenu::OnDrawItem(LPDRAWITEMSTRUCT  dis)
{
    if (dis->CtlType != ODT_MENU)
        return;

    MenuItemData* item_data = (MenuItemData*)dis->itemData;
    if (!item_data || item_data->menu != (HMENU)dis->hwndItem)
        return;

    // status
    bool is_checked = (dis->itemState & ODS_CHECKED) != 0;
    bool is_selected = (dis->itemState & ODS_SELECTED) != 0;
    bool is_grayed = (dis->itemState & ODS_DISABLED) ||
        (dis->itemState & ODS_GRAYED);
    bool is_default = (dis->itemState & ODS_DEFAULT) != 0;

    Gdiplus::Rect item_rect(dis->rcItem.left, dis->rcItem.top,
        dis->rcItem.right - dis->rcItem.left,
        dis->rcItem.bottom - dis->rcItem.top);
    Gdiplus::Rect mem_rect(0, 0, item_rect.Width, item_rect.Height);

    // memory dc
    HDC mem_dc = ::CreateCompatibleDC(dis->hDC);
    HBITMAP mem_bmp = ::CreateCompatibleBitmap(dis->hDC,
        item_rect.Width, item_rect.Height);
    HBITMAP old_bmp = (HBITMAP)SelectObject(mem_dc, mem_bmp);

    Gdiplus::Graphics graphics(mem_dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

    // draw background
    Gdiplus::SolidBrush back_brush(kColorNormalBack);
    Gdiplus::Rect back_rect(mem_rect.GetLeft() - 1, mem_rect.GetTop() - 1,
        item_rect.Width + 2, item_rect.Height + 2);
    graphics.FillRectangle(&back_brush, back_rect);

    Gdiplus::Rect draw_rect = mem_rect;
    if (item_data->index == 0)
        draw_rect.Y += kBorderHeight;

    int item_count = ::GetMenuItemCount(item_data->menu);
    if (item_data->index == item_count - 1)
        draw_rect.Height -= kBorderHeight;

    // focus border
    if (is_selected && !is_grayed &&
        (item_data->type & MF_SEPARATOR) == 0) {
        Gdiplus::SolidBrush hSelectedBackBrush(kColorSelectedBack);
        Gdiplus::Rect rect(draw_rect.GetLeft(), draw_rect.GetTop(),
            draw_rect.Width, draw_rect.Height);
        graphics.FillRectangle(&hSelectedBackBrush, rect);
    }

    // main part
    if (item_data->type & MF_SEPARATOR) {
        // separator type
        Gdiplus::Pen separator_pen(kColorSeparatorBack);
        Gdiplus::Point pt1(draw_rect.GetLeft() + 1,
            draw_rect.GetTop() + kSeparatorHeight / 2);
        Gdiplus::Point pt2(pt1.X + draw_rect.Width - 2, pt1.Y + 0.5);
        graphics.DrawLine(&separator_pen, pt1, pt2);
    } else {
        // string type
        // icon
        Gdiplus::Rect icon_rect(draw_rect.GetLeft() + kItemIconLeft,
            draw_rect.GetTop() + kItemIconTop,
            kItemIconWidth, kItemIconHeight);

        if (is_checked) {
            if ((item_data->type & MF_USECHECKBITMAPS)) {
                // checked icon: MF_USECHECKBITMAPS type - draw a ellipse
                Gdiplus::REAL center_x = icon_rect.GetLeft() +
                    icon_rect.Width / 2;
                Gdiplus::REAL center_y = icon_rect.GetTop() +
                    icon_rect.Height / 2;

                Gdiplus::Color ellipse_color;
                if (is_grayed)
                    ellipse_color = kColorDisabledText;
                else
                    ellipse_color = kColorNormalCheck;
                Gdiplus::Pen pen(ellipse_color, 2);
                Gdiplus::SolidBrush brush(ellipse_color);
                graphics.FillEllipse(&brush, center_x - kCheckEllipseRadis,
                    center_y - kCheckEllipseRadis, kCheckEllipseRadis * 2,
                    kCheckEllipseRadis * 2);
            } else {
                // checked icon - draw a check
                int origin_x = icon_rect.GetLeft() +
                    (icon_rect.Width - kCheckWidth) / 2;
                int origin_y = icon_rect.GetTop() +
                    (icon_rect.Height - kCheckHeight) / 2;

                Gdiplus::Color check_color;
                if (is_grayed)
                    check_color = kColorDisabledText;
                else
                    check_color = kColorNormalCheck;
                Gdiplus::Pen text_pen(check_color, kCheckPenSize);
                Gdiplus::Point ptsLines[3];
                static int dpi_inc = round(kDPI) - 1;
                ptsLines[0].X = origin_x + 0 - dpi_inc;
                ptsLines[0].Y = origin_y + 6 + dpi_inc;
                ptsLines[1].X = origin_x + 3;
                ptsLines[1].Y = origin_y + 9 + dpi_inc * 2;
                ptsLines[2].X = origin_x + 9 + dpi_inc;
                ptsLines[2].Y = origin_y + 1 - dpi_inc;
                graphics.DrawLines(&text_pen, ptsLines, 3);
            }
        }

        // text
        std::wstring text = item_data->text;
        unsigned shortcut_index = item_data->text.rfind(L'\t');
        std::wstring shortcut;
        if (shortcut_index != std::wstring::npos) {
            shortcut = text.substr(shortcut_index + 1);
            text = text.substr(0, shortcut_index);
        }
        Gdiplus::FontFamily family(kDefaultFontFamily.c_str());
        Gdiplus::Font font(&family, kDefaultFontSize,
            is_default ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular);
        Gdiplus::StringFormat stringformat;
        stringformat.SetAlignment(Gdiplus::StringAlignmentNear);
        stringformat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        stringformat.SetHotkeyPrefix(Gdiplus::HotkeyPrefixShow);

        Gdiplus::Color text_color;
        if (is_grayed)
            text_color = kColorDisabledText;
        else if (is_selected == true)
            text_color = kColorSelectedText;
        else
            text_color = kColorNormalText;
        Gdiplus::SolidBrush text_brush(text_color);

        Gdiplus::RectF text_rect(
            (Gdiplus::REAL)(draw_rect.GetLeft() + kMenuLeftWidth),
            (Gdiplus::REAL)(draw_rect.GetTop()),
            (Gdiplus::REAL)
            (draw_rect.Width - kMenuLeftWidth - kMenuRightWidth),
            (Gdiplus::REAL)(kMenuLeftHeight));

        graphics.DrawString(text.c_str(), text.length(), &font,
            text_rect, &stringformat, &text_brush);
        if (!shortcut.empty()) {
            // shortcut text
            stringformat.SetAlignment(Gdiplus::StringAlignmentFar);
            Gdiplus::SolidBrush shortcut_brush(kColorShortCutText);
            graphics.DrawString(shortcut.c_str(), shortcut.length(), &font,
                text_rect, &stringformat, &shortcut_brush);
        }

        // arrow
        if (item_data->sub_menu != NULL) {
            const int nArrowPointCount = 4;
            Gdiplus::Point pts[nArrowPointCount];
            pts[0].X = mem_rect.GetRight() - kArrowRightSpace;
            pts[0].Y = mem_rect.GetTop() + item_rect.Height / 2;
            pts[1].X = pts[0].X - kArrowWidth;
            pts[1].Y = pts[0].Y + kArrowHeight / 2;
            pts[2].X = pts[0].X - kArrowWidth;
            pts[2].Y = pts[0].Y - kArrowHeight / 2;
            pts[3].X = pts[0].X;
            pts[3].Y = pts[0].Y;

            Gdiplus::Color arraw_color;
            if (is_grayed)
                arraw_color = kColorDisabledText;
            else
                arraw_color = kColorNormalText;
            Gdiplus::SolidBrush arraw_brush(arraw_color);
            graphics.FillPolygon(&arraw_brush, pts, nArrowPointCount);
        }
    }

    // copy memory to dc
    ::BitBlt(dis->hDC, item_rect.GetLeft(), item_rect.GetTop(),
        item_rect.Width, item_rect.Height, mem_dc,
        mem_rect.GetLeft(), mem_rect.GetTop(), SRCCOPY);

    // exclude the system arrow area
    if (item_data->sub_menu != NULL)
        ExcludeClipRect(dis->hDC, item_rect.GetLeft(), item_rect.GetTop(),
        item_rect.GetRight(), item_rect.GetBottom());

    ::SelectObject(mem_dc, old_bmp);
    ::DeleteDC(mem_dc);
    ::DeleteObject(mem_bmp);
}

void MxCustomMenu::OnMeasureItem(LPMEASUREITEMSTRUCT mis)
{
    if (mis->CtlType != ODT_MENU) return;

    MenuItemData* item_data = (MenuItemData *)mis->itemData;
    if (item_data == NULL)
        return;

    int width = 0;
    int height = 0;

    if (item_data->type & MF_SEPARATOR) {
        width = mis->itemWidth > (UINT)kMenuItemWidth ?
            mis->itemWidth : kMenuItemWidth;
        height = kSeparatorHeight;
    } else {
        HDC hdc = GetDC(NULL);
        SIZE SizeString;
        ::GetTextExtentPoint32(hdc, item_data->text.c_str(),
            item_data->text.length(), &SizeString);
        int need_width = SizeString.cx + kMenuLeftWidth + kMenuRightWidth;
        width = need_width > kMenuItemWidth ? need_width : kMenuItemWidth;
        height = kMenuItemHeight;

        ReleaseDC(NULL, hdc);
    }
    // border height
    int item_count = ::GetMenuItemCount(item_data->menu);
    if (item_data->index == 0 || item_data->index == item_count - 1)
        height += kBorderHeight;

    //setting the info
    mis->itemWidth = width;
    mis->itemHeight = height;
}

bool MxCustomMenu::EnableOwnerDraw(HMENU menu, MenuItemData* item_data)
{
    if (menu && item_data) {
        item_data->sub_datas.clear();

        int item_count = ::GetMenuItemCount(menu);
        for (int i = 0; i < item_count; i++)
        {
            // make sure we do not change the state of the menu items as
            // we set the owner drawn style
            MENUITEMINFO item_info;
            memset(&item_info, 0, sizeof(MENUITEMINFO));
            item_info.cbSize = sizeof(MENUITEMINFO);
            item_info.fMask = MIIM_STATE | MIIM_TYPE;
            if (!::GetMenuItemInfo(menu, i, TRUE, &item_info))
                return false;

            int itemID = ::GetMenuItemID(menu, i);
            BOOL succ = FALSE;
            MenuItemData* sub_item_data = new MenuItemData;
            item_data->sub_datas.push_back(sub_item_data);

            if ((item_info.fType & MFT_SEPARATOR) == 0) {
                TCHAR* item_text = new TCHAR[item_info.cch + 1];
                item_text[item_info.cch] = '\0';
                ++item_info.cch;
                succ = ::GetMenuString(menu, i, item_text,
                    item_info.cch, MF_BYPOSITION);
                if (succ)
                    sub_item_data->text = item_text;

                delete item_text;
                item_text = NULL;

                if (!succ)
                    return false;

                succ = ::ModifyMenu(menu, i,
                    item_info.fState | MF_BYPOSITION | MF_OWNERDRAW,
                    itemID, (LPCTSTR)sub_item_data);
            } else {
                succ = ::ModifyMenu(menu, i,
                    MF_BYPOSITION | MF_OWNERDRAW | MF_SEPARATOR,
                    itemID, (LPCTSTR)sub_item_data);
            }
            if (!succ)
                return false;

            sub_item_data->menu = menu;
            sub_item_data->index = i;
            sub_item_data->type = item_info.fType;
            HMENU sub_menu = ::GetSubMenu(menu, i);
            if (sub_menu) {
                sub_item_data->sub_menu = sub_menu;
                if (!EnableOwnerDraw(sub_menu, sub_item_data))
                    return false;
            }
        }
        return true;
    }
    return false;
}

bool MxCustomMenu::HookOwnerWnd(HWND hwnd)
{
    if (hook_map_.find(hwnd) == hook_map_.end()) {
        owner_wnd_ = hwnd;
        def_wnd_proc_ = (WNDPROC)::SetWindowLong(hwnd,
            GWL_WNDPROC, (LONG)OwnerWndProc);
        hook_map_[hwnd] = def_wnd_proc_;
    } else {
        def_wnd_proc_ = hook_map_[hwnd];
    }

    ::SetProp(hwnd, L"MxCustomMenu", this);
    return NULL != def_wnd_proc_;
}

void MxCustomMenu::UnHookOwnerWnd()
{
    if (owner_wnd_) {
        ::SetWindowLong(owner_wnd_, GWL_WNDPROC, (LONG)def_wnd_proc_);
        hook_map_.erase(owner_wnd_);
        owner_wnd_ = NULL;
        def_wnd_proc_ = NULL;
    }
}

LRESULT CALLBACK MxCustomMenu::OwnerWndProc(HWND hwnd, UINT message,
    WPARAM w_param, LPARAM l_param)
{
    MxCustomMenu* context_menu = static_cast<MxCustomMenu*>(
        ::GetProp(hwnd, kOwnerMenuPropName.c_str()));
    if (!context_menu)
        return 0;

    switch (message)
    {
    case WM_DRAWITEM:
    {
        context_menu->OnDrawItem((DRAWITEMSTRUCT*)l_param);
        break;
    }
    break;
    case WM_MEASUREITEM:
    {
        context_menu->OnMeasureItem((MEASUREITEMSTRUCT*)l_param);
    }
    break;
    default:
        break;
    }
    return hook_map_[hwnd](hwnd, message, w_param, l_param);
}
