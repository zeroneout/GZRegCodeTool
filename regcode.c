// 序号转换工具 —— 纯 Win32 C 实现（Unicode/W 版，无框架、无依赖）
// 编译: clang -O2 -s -static -mwindows regcode.c resource.o -o regcode.exe
// 运行环境: Windows 7 SP1 ~ Windows 11
// 说明: 全程使用 W(宽字符/Unicode) 版 API 与 L"..." 字面量，避免 ANSI 代码页导致的乱码。
#include <windows.h>

// 本工具链默认链接 UCRT（api-ms-win-crt-*），而 Windows 7 上没有这些 DLL，
// 故不链接 C 运行时，自行提供 memcpy/memset；其余字符串/内存函数走 kernel32。
// 这样 exe 只依赖 GDI32/KERNEL32/USER32 三个系统自带 DLL，Win7~Win11 均可运行。
void* memcpy(void* dst, const void* src, size_t n)
{
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}
void* memset(void* dst, int c, size_t n)
{
    unsigned char* d = (unsigned char*)dst;
    for (size_t i = 0; i < n; i++) d[i] = (unsigned char)c;
    return dst;
}

// 固定的 8 字符查找表（按需求提供），全部为 ASCII，转宽字符无损失
static const char TBL[8] = "9C1VKP36";

// 布局常量（集中管理，方便统一调整；整体相对初版放大 10%）
#define CONTENT_W 338   // 内容区总宽（上下两行一致，保证左右对齐）
#define BOX_W     110   // 输入框/输出框宽度
#define BOX_H     24    // 输入框/输出框高度
#define LBL_W     52    // “序号：”“结果：”标签宽度
#define GAP_L      4    // 标签与输入框之间的间隙
#define GAP_M      6    // 两列（输入列 / 结果列）之间的间隙

static const int BTN_CONVERT = 1;
static const int BTN_ABOUT   = 2;
static const int BTN_EXIT    = 3;

// 把整数控件 ID 安全地转成 HMENU（消除 int->pointer 警告）
#define BTN(id) ((HMENU)(UINT_PTR)(id))

// 应用图标资源 ID（与 resource.rc 中一致）
#define IDI_APPICON 100

// 「关于」信息（点击“关于”后在原窗口内显示，不弹新窗、不发声）
static HINSTANCE g_hInst    = NULL;   // 模块句柄（加载图标用）
static int       g_showAbout = 0;     // 是否处于“关于”展示态（1=显示关于，0=正常界面）
static const wchar_t ABOUT_TEXT[] =
    L"该程序由workbuddy帮助构建，构建日期2026年10月26日。";

static HWND g_hMain      = NULL;   // 主窗口句柄（自绘输出框重绘/复制时需要）
static HWND g_hInput     = NULL;   // 输入框
static HWND g_hLblSerial = NULL;   // “序号：”标签
static HWND g_hLblResult = NULL;   // “结果：”标签
static HWND g_hBtnConvert= NULL;   // 转换按钮
static HWND g_hBtnAbout  = NULL;   // 关于按钮
static HWND g_hBtnExit   = NULL;   // 退出按钮
static HFONT g_hFont     = NULL;   // 清晰字体（微软雅黑，替代默认点阵字体，避免文字发虚）
static WNDPROC g_oldEditProc = NULL;   // 输入框原窗口过程（子类化用）
static wchar_t g_result[64] = { 0 };   // 输出结果（自绘区域显示）
static RECT g_rcInput  = { 0 };        // 输入框矩形（客户区坐标，边框由父窗口绘制）
static RECT g_rcResult = { 0 };        // 输出框矩形（客户区坐标，边框由父窗口绘制）

// 创建界面字体：微软雅黑（TrueType + ClearType）。输入/按钮/标签用雅黑都正常；
// 输出框已改为自绘，不再受任何控件文本渲染影响，故可放心用雅黑获得清晰字形。
// dpi 参数用于按当前缩放比例计算字号，保证高分屏不变形。
static HFONT CreateUiFont(int dpi)
{
    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = -MulDiv(10, dpi, 72);   // 10pt（相对初版放大 10%），随 DPI 适配
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;  // 强制 TrueType，禁用点阵回退
    lf.lfQuality = CLEARTYPE_QUALITY;    // 清晰抗锯齿
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    lstrcpyW(lf.lfFaceName, L"Microsoft YaHei");   // 系统 API，避免静态拉入 C 运行时字符串函数
    return CreateFontIndirectW(&lf);
}

// 创建子控件并立即套用清晰字体，避免到处重复书写 WM_SETFONT
static HWND mk(HWND parent, LPCWSTR cls, LPCWSTR txt, DWORD style,
               int x, int y, int w, int h, HMENU id)
{
    HWND c = CreateWindowW(cls, txt, style, x, y, w, h, parent, id, NULL, NULL);
    if (c && g_hFont)
        SendMessageW(c, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    return c;
}

// 按当前客户区宽度重新计算布局：居中、对齐、并套用字体。
// 创建时与 WM_DPICHANGED（跨屏 DPI 变化）时都会调用，实现布局与字号自适应。
static void Relayout(HWND hWnd, int dpi)
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    int bx = (rc.right - CONTENT_W) / 2;   // 整体居中，左右留白相等
    if (bx < 0) bx = 0;

    // 第一行：序号： + 输入框  |  结果： + 输出框（同一行，等长 BOX_W）
    int lblY = 24, boxY = 20;                       // 标签 / 框的顶部（相对客户区）
    int xSerial = bx + 0;
    int xInput  = bx + LBL_W + GAP_L;                // 标签右侧 + 间隙
    int xResult = xInput + BOX_W + GAP_M;            // 输入列右侧 + 间隙
    int xOutput = xResult + LBL_W + GAP_L;           // 结果标签右侧 + 间隙

    MoveWindow(g_hLblSerial,  xSerial, lblY, LBL_W, 22, TRUE);
    // 输入框不带 WS_BORDER、内缩 2px，边框由父窗口 WM_PAINT 统一用 DrawEdge 绘制，
    // 这样输入框与输出框外观完全一致（均为白底 + 凹陷边框）。
    MoveWindow(g_hInput,      xInput + 2, boxY + 2, BOX_W - 4, BOX_H - 4, TRUE);
    MoveWindow(g_hLblResult,  xResult, lblY, LBL_W, 22, TRUE);

    // 第二行：三个按钮，与上方内容等宽对齐（总宽同样 CONTENT_W，间距 13）
    int btnY = 64, btnH = 33, btnW = 104, gap = 13;
    MoveWindow(g_hBtnConvert, bx + 0,               btnY, btnW, btnH, TRUE);
    MoveWindow(g_hBtnAbout,   bx + btnW + gap,       btnY, btnW, btnH, TRUE);
    MoveWindow(g_hBtnExit,    bx + 2 * (btnW + gap), btnY, btnW, btnH, TRUE);

    // 记录两个框的矩形（供 WM_PAINT 自绘边框 / 双击命中判定）
    SetRect(&g_rcInput,  xInput,  boxY, xInput + BOX_W,  boxY + BOX_H);
    SetRect(&g_rcResult, xOutput, boxY, xOutput + BOX_W, boxY + BOX_H);

    // 字体应用到所有控件
    if (g_hFont)
    {
        SendMessageW(g_hLblSerial,  WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessageW(g_hLblResult,  WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessageW(g_hInput,      WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessageW(g_hBtnConvert, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessageW(g_hBtnAbout,   WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessageW(g_hBtnExit,    WM_SETFONT, (WPARAM)g_hFont, TRUE);
    }
    InvalidateRect(hWnd, NULL, TRUE);   // 整窗重绘（含两个自绘框）
}

// 前向声明：EditProc 会调用这两个函数（其定义在文件稍后）
static void do_convert(void);
static void copy_result(HWND hWnd);

// 输入框子类过程：拦截回车（直接转换）、Ctrl+C（未选中输入框文本时复制结果），
// 以及「已满 8 位再敲数字时取消编辑框提示音」。
static LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_KEYDOWN)
    {
        if (wp == VK_RETURN)          // 回车即转换
        {
            do_convert();
            return 0;                 // 吞掉回车，避免编辑框默认行为/提示音
        }
        if (wp == 'C' && (GetKeyState(VK_CONTROL) & 0x8000))
        {
            DWORD sel = (DWORD)SendMessageW(hwnd, EM_GETSEL, 0, 0);
            if (LOWORD(sel) == HIWORD(sel))   // 未选中输入框文本时，Ctrl+C 复制结果
            {
                copy_result(g_hMain);
                return 0;
            }
            // 有选中则交给默认处理（复制输入框文本）
        }
    }
    if (msg == WM_CHAR)
    {
        // 数字键：当输入框已无剩余空间（扣除选中文本长度后已满 8 位）时，
        // 直接吞掉该字符、不再交给控件处理，从而取消编辑框的「超过限制」提示音。
        if (wp >= '0' && wp <= '9')
        {
            DWORD sel = (DWORD)SendMessageW(hwnd, EM_GETSEL, 0, 0);
            DWORD selLen = HIWORD(sel) - LOWORD(sel);
            DWORD len = (DWORD)GetWindowTextLengthW(hwnd);
            if (len - selLen >= 8)
                return 0;   // 无空间可输入：吞掉字符，避免提示音
        }
    }
    return CallWindowProcW(g_oldEditProc, hwnd, msg, wp, lp);
}

// 核心算法（移植自原 Python，已去掉写注册表部分）
// serial 为输入数字；每轮取 TBL[serial % 8] 后将 serial /= 8；循环 10 次
static void reg_code(int serial, wchar_t* out)
{
    for (int i = 0; i < 10; i++)
    {
        out[i] = (wchar_t)TBL[serial % 8];
        serial /= 8;
    }
    out[10] = L'\0';
}

// 设置输出结果（自绘区域显示）：写入字符串后让主窗口重绘输出框矩形。
// 输出框由 WM_PAINT 手绘，不依赖任何控件的文本渲染，绝不会出现叠加/压字。
static void set_result(const wchar_t* txt)
{
    lstrcpynW(g_result, txt, (int)(sizeof(g_result) / sizeof(wchar_t)));
    InvalidateRect(g_hMain, &g_rcResult, TRUE);
}

// 把当前结果复制到剪贴板（双击输出框 / Ctrl+C 时调用，弥补自绘区域不可选中的不足）
static void copy_result(HWND hWnd)
{
    if (g_result[0] == L'\0') return;
    if (!OpenClipboard(hWnd)) return;
    EmptyClipboard();
    SIZE_T sz = (SIZE_T)(lstrlenW(g_result) + 1) * sizeof(wchar_t);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, sz);
    if (h)
    {
        wchar_t* p = (wchar_t*)GlobalLock(h);
        if (p)
        {
            memcpy(p, g_result, sz);
            GlobalUnlock(h);
            if (!SetClipboardData(CF_UNICODETEXT, h))
                GlobalFree(h);   // 写入失败则回收内存，避免泄漏
        }
        else GlobalFree(h);
    }
    CloseClipboard();
}

static void do_convert(void)
{
    wchar_t buf[32];
    GetWindowTextW(g_hInput, buf, (int)(sizeof(buf) / sizeof(wchar_t)));

    int len = lstrlenW(buf);
    int ok = (len == 8);
    for (int i = 0; ok && i < len; i++)
        if (buf[i] < L'0' || buf[i] > L'9')
            ok = 0;

    if (!ok)
    {
        set_result(L"请输入有效的8位数字");
        return;
    }

    int serial = 0;
    for (int i = 0; i < 8; i++)
        serial = serial * 10 + (buf[i] - L'0');

    wchar_t res[16];
    reg_code(serial, res);
    set_result(res);
}

// 切换“关于”展示态：在原窗口内显示/隐藏关于界面，不弹新窗、不发声。
// 展示态下隐藏所有编辑控件，改由 WM_PAINT 绘制图标+说明；再次点击窗口或按 Esc 返回。
static void toggle_about(void)
{
    g_showAbout = !g_showAbout;
    int show = g_showAbout ? SW_HIDE : SW_SHOW;
    ShowWindow(g_hLblSerial,  show);
    ShowWindow(g_hInput,      show);
    ShowWindow(g_hLblResult,  show);
    ShowWindow(g_hBtnConvert, show);
    ShowWindow(g_hBtnAbout,   show);
    ShowWindow(g_hBtnExit,    show);
    if (g_showAbout)
        SetFocus(g_hMain);   // 展示态下让主窗口获得焦点，便于接收 Esc/键盘返回
    InvalidateRect(g_hMain, NULL, TRUE);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        g_hMain = hWnd;   // 记录主窗口句柄，供自绘输出框重绘/复制使用

        HDC hdc = GetDC(hWnd);
        int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(hWnd, hdc);
        g_hFont = CreateUiFont(dpi);

        // 创建控件（先给占位尺寸，随后由 Relayout 精确定位）
        g_hLblSerial  = mk(hWnd, L"STATIC", L"序号：", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, NULL);
        g_hInput      = mk(hWnd, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER, 0, 0, 10, 10, NULL);
        // 限制最多 8 位数字（配合 ES_NUMBER，超出直接输不进去）
        SendMessageW(g_hInput, EM_LIMITTEXT, 8, 0);
        g_hLblResult  = mk(hWnd, L"STATIC", L"结果：", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, NULL);
        g_hBtnConvert = mk(hWnd, L"BUTTON", L"转换", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 10, 10, BTN(BTN_CONVERT));
        g_hBtnAbout   = mk(hWnd, L"BUTTON", L"关于", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, BTN(BTN_ABOUT));
        g_hBtnExit    = mk(hWnd, L"BUTTON", L"退出", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, BTN(BTN_EXIT));

        // 子类化输入框：拦截回车与 Ctrl+C
        g_oldEditProc = (WNDPROC)SetWindowLongPtrW(g_hInput, GWLP_WNDPROC, (LONG_PTR)EditProc);

        Relayout(hWnd, dpi);
        return 0;
    }

    case WM_DPICHANGED:
    {
        // 窗口被拖到不同缩放比例的显示器：按新 DPI 重建字体并重排，保证清晰且布局正确
        int newDpi = (int)LOWORD(wParam);
        RECT* prc = (RECT*)lParam;   // 系统建议的新窗口矩形（已按新 DPI 缩放）
        SetWindowPos(hWnd, NULL, prc->left, prc->top,
                     prc->right - prc->left, prc->bottom - prc->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        if (g_hFont) { DeleteObject(g_hFont); g_hFont = NULL; }
        g_hFont = CreateUiFont(newDpi);
        Relayout(hWnd, newDpi);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
        // 让「序号：」「结果：」等静态文本背景透明，去掉灰色阴影方块
        SetBkMode((HDC)wParam, TRANSPARENT);
        return (LRESULT)(HBRUSH)GetStockObject(NULL_BRUSH);

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // 「关于」展示态：在原窗口内绘制图标 + 说明 + 返回提示（不弹新窗、不发声）
        if (g_showAbout)
        {
            RECT rc; GetClientRect(hWnd, &rc);
            SetBkMode(hdc, TRANSPARENT);
            HFONT old = (HFONT)SelectObject(hdc, g_hFont);
            // 应用图标（可爱粉嫩图标）居中上方
            HICON icon = LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_APPICON));
            if (icon)
            {
                DrawIconEx(hdc, (rc.right - 40) / 2, 16, icon, 40, 40, 0, NULL, DI_NORMAL);
                DestroyIcon(icon);
            }
            // 说明文字（自动换行、居中）
            RECT tr = { 16, 62, rc.right - 16, 128 };
            DrawTextW(hdc, ABOUT_TEXT, -1, &tr, DT_CENTER | DT_WORDBREAK);
            // 返回提示
            RECT pr2 = { 16, 132, rc.right - 16, 154 };
            DrawTextW(hdc, L"点击窗口任意处或按 Esc 返回", -1, &pr2, DT_CENTER | DT_SINGLELINE);
            SelectObject(hdc, old);
            EndPaint(hWnd, &ps);
            return 0;
        }

        // 正常界面：输入框与输出框统一手绘白底 + 凹陷边框。输出框文字也在此自绘，
        // 完全不依赖任何控件的文本渲染，因此不可能出现字符重叠/叠加/伪字符。

        // 两个框先用同样的白底 + 凹陷边框绘制（外观完全一致）
        FillRect(hdc, &g_rcInput,  (HBRUSH)GetStockObject(WHITE_BRUSH));
        DrawEdge(hdc, &g_rcInput,  EDGE_SUNKEN, BF_RECT);
        FillRect(hdc, &g_rcResult, (HBRUSH)GetStockObject(WHITE_BRUSH));
        DrawEdge(hdc, &g_rcResult, EDGE_SUNKEN, BF_RECT);

        // 输出框文字（自绘、居中）。输入框文字由 EDIT 控件自己绘制，已加 ES_CENTER 同样居中。
        SetBkMode(hdc, TRANSPARENT);
        HFONT old = (HFONT)SelectObject(hdc, g_hFont);
        RECT tr = g_rcResult;
        InflateRect(&tr, -3, -3);   // 文字内缩，避开边框
        DrawTextW(hdc, g_result, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
        if (g_showAbout)            // 关于展示态：点击窗口任意处返回正常界面
        {
            toggle_about();
            return 0;
        }
        break;

    case WM_LBUTTONDBLCLK:
    {
        // 双击输出框区域即复制结果到剪贴板（仅正常界面生效）
        if (g_showAbout) return 0;
        POINT pt;
        pt.x = (LONG)(short)LOWORD(lParam);
        pt.y = (LONG)(short)HIWORD(lParam);
        if (PtInRect(&g_rcResult, pt))
            copy_result(hWnd);
        return 0;
    }

    case WM_KEYDOWN:
        if (g_showAbout && (wParam == VK_ESCAPE || wParam == VK_RETURN))
        {                       // 关于展示态：Esc / 回车 返回正常界面
            toggle_about();
            return 0;
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == BTN_CONVERT)        // 转换
            do_convert();
        else if (LOWORD(wParam) == BTN_ABOUT)     // 关于（原窗口内切换展示态）
            toggle_about();
        else if (LOWORD(wParam) == BTN_EXIT)      // 退出
            DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        if (g_hFont) DeleteObject(g_hFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// 自定义入口（不依赖 CRT 启动代码，避免 UCRT 依赖）：入口固定为 app_entry。
void app_entry(void)
{
    HINSTANCE hInst = GetModuleHandleW(NULL);
    g_hInst = hInst;   // 记录模块句柄，供关于框注册窗口类 / 加载图标使用
    SetProcessDPIAware();   // 无 manifest 时仍声明 DPI 感知，保证高分屏文字清晰

    static const wchar_t CLASS[] = L"RegCodeToolW";
    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.style = CS_DBLCLKS;   // 接收双击消息（用于双击输出框复制）
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);  // 客户区背景刷，避免自绘时擦除异常
    wc.lpszClassName = CLASS;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APPICON));
    RegisterClassW(&wc);

    HWND hWnd = CreateWindowW(
        CLASS, L"工资序号转换工具",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 396, 160,
        NULL, NULL, hInst, NULL);
    ShowWindow(hWnd, SW_SHOWDEFAULT);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    ExitProcess(0);
}
