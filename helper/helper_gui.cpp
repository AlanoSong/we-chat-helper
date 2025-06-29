#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <iostream>
#include <TlHelp32.h>
#include <Psapi.h>

#include "..\share\share.h"

#include "config.h"
#include "utils.h"
#include "socket_server.h"

//============================================================================================================
// Global variables
//============================================================================================================
static HANDLE g_hWechatProcess = NULL;
static HMODULE g_injectedDllBase = NULL;

static std::wstring g_lastDllPath;
static std::wstring g_lastProcName;

static BOOL g_isInjected = FALSE;
static BOOL g_isDllFuncCalled = FALSE;
static HANDLE g_hSocketThread = NULL;

// Function table to inform spy dll which one to hook
static SPY_CONTEXT g_spyContext =
{
    {
        { FUNC_TYPE_LOG_MSG,    FALSE },
        { FUNC_TYPE_SEND_MSG,   FALSE },
        { FUNC_TYPE_RECV_MSG,   FALSE },
    },
    FUNC_TOTAL_NUM,
    SOCKET_SERVER_PORT,
};

//============================================================================================================
// GUI functions
//============================================================================================================
// GUI message IDs
#define IDC_EDIT_DLLPATH        1001
#define IDC_BUTTON_INJECT       1002
#define IDC_BUTTON_EJECT        1003
#define IDC_EDIT_PROCNAME       1004
#define IDC_STATIC_RESULT       1005
#define IDC_BUTTON_CLEARLOG     1006
#define IDC_CHECK_BOX_BASE      1100 // + FUNC_TOTAL_NUM
#define WM_SOCKET_MSG           (WM_USER + 200)

// Edit controls and buttons
static HWND g_hEditDllPath;
static HWND g_hEditProcName;
// Status bar
static HWND g_hStatusBar = NULL;
// Font for UI elements
static HFONT g_hFont = NULL;
// Log msg output
static HWND g_hLogEdit = NULL;
// Socket handler
SOCKET g_recvSock = INVALID_SOCKET;

static void SetUIFont(HWND hwnd, HFONT hFont);
static void InitFuncSwitchCheckboxes(HWND hDlg);
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

//============================================================================================================
// Main function
//============================================================================================================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);

    const wchar_t CLASS_NAME[] = L"InjectorWindowClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    std::wstring appName = INJECTOR_NAME;
    std::wstring appVersion = INJECTOR_VERSION;
    std::wstring appInfo = appName + L" " + appVersion;

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        appInfo.c_str(),
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        800,
        600,
        NULL,
        NULL,
        hInstance,
        NULL);

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

//============================================================================================================
// Static functions
//============================================================================================================
static void SetUIFont(HWND hwnd, HFONT hFont)
{
    SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
}

static void InitFuncSwitchCheckboxes(HWND hDlg)
{
    int baseX = 130;
    int baseY = 90;
    int deltaX = 120;

    for (int i = 0; i < FUNC_TOTAL_NUM; ++i)
    {
        HWND hCheck = CreateWindow(
            L"BUTTON",
            (i == FUNC_TYPE_LOG_MSG) ? L"Log Msg" :
            (i == FUNC_TYPE_SEND_MSG) ? L"Send Msg" :
            (i == FUNC_TYPE_RECV_MSG) ? L"Recv Msg" : L"Unknown",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            baseX + i * deltaX, baseY, 100, 24,
            hDlg,
            (HMENU)(IDC_CHECK_BOX_BASE + i),
            (HINSTANCE)GetWindowLongPtr(hDlg, GWLP_HINSTANCE),
            NULL);

        SetUIFont(hCheck, g_hFont);

        SendMessage(hCheck,
            BM_SETCHECK,
            g_spyContext.funcList[i].tryHookThis ? BST_CHECKED : BST_UNCHECKED,
            0);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        LOGFONTW lf = { 0 };
        lf.lfHeight = 18;
        wcscpy_s(lf.lfFaceName, L"Segoe UI");
        g_hFont = CreateFontIndirectW(&lf);

        HWND hProcTitle = CreateWindowW(
            L"STATIC",
            L"Process Name:",
            WS_VISIBLE | WS_CHILD,
            20, 20, 100, 20,
            hwnd,
            NULL,
            NULL,
            NULL);
        g_hEditProcName = CreateWindowW(L"EDIT",
            INJECT_EXE,
            WS_VISIBLE | WS_CHILD | WS_BORDER,
            130, 20, 200, 20,
            hwnd,
            (HMENU)IDC_EDIT_PROCNAME,
            NULL,
            NULL);
        SetUIFont(hProcTitle, g_hFont);
        SetUIFont(g_hEditProcName, g_hFont);

        WCHAR dirPath[MAX_PATH] = { 0 };
        GetCurrentDirectoryW(MAX_PATH, dirPath);
        std::wstring dllPath = dirPath + std::wstring(L"\\") + std::wstring(INJECT_DLL);

        HWND hDllTitle = CreateWindowW(L"STATIC", L"Spy Dll Path:", WS_VISIBLE | WS_CHILD,
            20, 55, 100, 20, hwnd, NULL, NULL, NULL);
        g_hEditDllPath = CreateWindowW(L"EDIT", dllPath.c_str(), WS_VISIBLE | WS_CHILD | WS_BORDER,
            130, 55, 400, 20, hwnd, (HMENU)IDC_EDIT_DLLPATH, NULL, NULL);
        SetUIFont(hDllTitle, g_hFont);
        SetUIFont(g_hEditDllPath, g_hFont);

        HWND hInjectBut = CreateWindowW(L"BUTTON", L"Inject", WS_VISIBLE | WS_CHILD,
            130, 130, 80, 30, hwnd, (HMENU)IDC_BUTTON_INJECT, NULL, NULL);
        HWND hEjectBut = CreateWindowW(L"BUTTON", L"Eject", WS_VISIBLE | WS_CHILD,
            230, 130, 80, 30, hwnd, (HMENU)IDC_BUTTON_EJECT, NULL, NULL);
        HWND hClearLogBut = CreateWindowW(L"BUTTON", L"Clear Log", WS_VISIBLE | WS_CHILD,
            330, 130, 100, 30, hwnd, (HMENU)IDC_BUTTON_CLEARLOG, NULL, NULL);

        SetUIFont(hInjectBut, g_hFont);
        SetUIFont(hEjectBut, g_hFont);
        SetUIFont(hClearLogBut, g_hFont);

        InitFuncSwitchCheckboxes(hwnd);

        g_hStatusBar = CreateWindowExW(
            0,
            STATUSCLASSNAMEW,
            NULL,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0,
            hwnd,
            NULL,
            NULL,
            NULL);

        int statwidths[] = { 600, -1 };
        SendMessageW(g_hStatusBar, SB_SETPARTS, 1, (LPARAM)statwidths);
        SendMessageW(g_hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"Ready");

        g_hLogEdit = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            20, 170, 760, 380,
            hwnd,
            NULL,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL
        );
        SetUIFont(g_hLogEdit, g_hFont);

        SOCKET_SERVER_INFO srvInfo = { 0 };
        srvInfo.msgId = WM_SOCKET_MSG;
        srvInfo.listenPort = SOCKET_SERVER_PORT;
        srvInfo.hWindow = hwnd;
        g_hSocketThread = CreateThread(NULL, 0, SocketServerThread, &srvInfo, 0, NULL);

        break;
    }

    case WM_COMMAND:
    {
        int msgId = LOWORD(wParam);
        // Handle check box event
        if (msgId >= IDC_CHECK_BOX_BASE && msgId < IDC_CHECK_BOX_BASE + FUNC_TOTAL_NUM)
        {
            int idx = msgId - IDC_CHECK_BOX_BASE;
            BOOL checked = (IsDlgButtonChecked(hwnd, msgId) == BST_CHECKED);
            g_spyContext.funcList[idx].tryHookThis = checked;
            break;
        }

        // Handle clear log button event
        if (msgId == IDC_BUTTON_CLEARLOG)
        {
            if (g_hLogEdit)
            {
                SetWindowTextW(g_hLogEdit, L"");
            }
            break;
        }

        // Handle inject button event
        if (msgId == IDC_BUTTON_INJECT)
        {
            wchar_t procName[256] = { 0 };
            wchar_t dllPath[512] = { 0 };
            GetWindowTextW(g_hEditProcName, procName, 255);
            GetWindowTextW(g_hEditDllPath, dllPath, 511);

            DWORD pid = GetProcessIdByName(procName);
            if (pid == 0xFFFFFFFF || pid == 0)
            {
                SendMessageW(g_hStatusBar,
                    SB_SETTEXTW, 0, (LPARAM)L"Process not found!");
                break;
            }
            if (!g_isInjected && InjectSpyDll(pid, &g_hWechatProcess, dllPath, &g_injectedDllBase))
            {
                if (!g_isDllFuncCalled)
                {
                    g_isDllFuncCalled = CallDllFuncEx(g_hWechatProcess,
                        dllPath,
                        g_injectedDllBase,
                        "InstallHook",
                        &g_spyContext,
                        sizeof(SPY_CONTEXT),
                        NULL);

                    if (g_isDllFuncCalled)
                    {
                        SendMessageW(g_hStatusBar,
                            SB_SETTEXTW, 0, (LPARAM)L"Inject success!");
                    }
                    else
                    {
                        SendMessageW(g_hStatusBar,
                            SB_SETTEXTW, 0, (LPARAM)L"Install hook failed!");
                    }
                }

                g_lastDllPath = dllPath;
                g_lastProcName = procName;

                g_isInjected = TRUE;
            }
            else
            {
                SendMessageW(g_hStatusBar,
                    SB_SETTEXTW, 0, (LPARAM)L"Already injected or inject failed!");
            }
        }

        // Handle eject button event
        if (msgId == IDC_BUTTON_EJECT)
        {
            wchar_t procName[256] = { 0 };
            GetWindowTextW(g_hEditProcName, procName, 255);

            DWORD pid = GetProcessIdByName(procName);
            if (pid == 0xFFFFFFFF || pid == 0)
            {
                SendMessageW(g_hStatusBar,
                    SB_SETTEXTW, 0, (LPARAM)L"Process not found!");
                break;
            }

            if (g_isDllFuncCalled)
            {
                BOOL isCallFuncSuccess = CallDllFuncEx(g_hWechatProcess,
                    g_lastDllPath.c_str(),
                    g_injectedDllBase,
                    "RemoveHook",
                    NULL,
                    0,
                    NULL);

                if (!isCallFuncSuccess)
                {
                    SendMessageW(g_hStatusBar,
                        SB_SETTEXTW, 0, (LPARAM)L"Remove hook failed!");
                }

                g_isDllFuncCalled = FALSE;
            }

            if (g_isInjected)
            {
                if (EjectSpyDll(pid, g_lastDllPath.c_str()))
                {
                    SendMessageW(g_hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"Eject success!");
                }
                else
                {
                    SendMessageW(g_hStatusBar,
                        SB_SETTEXTW, 0, (LPARAM)L"Eject failed!");
                }

                g_isInjected = FALSE;
            }
        }
        break;
    }

    case WM_SIZE:
    {
        if (g_hStatusBar)
        {
            SendMessageW(g_hStatusBar, WM_SIZE, 0, 0);
        }
        if (g_hLogEdit)
        {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            int logTop = 170;
            int statusBarHeight = 30;
            int logHeight = rcClient.bottom - logTop - statusBarHeight;
            if (logHeight < 50) logHeight = 50;
            MoveWindow(g_hLogEdit, 20, logTop, rcClient.right - 40, logHeight, TRUE);
        }
        break;
    }
    case WM_DESTROY:
    {
        if (g_recvSock != INVALID_SOCKET)
        {
            closesocket(g_recvSock);
            g_recvSock = INVALID_SOCKET;
        }

        if (g_hSocketThread) {
            CloseHandle(g_hSocketThread);
            g_hSocketThread = NULL;
        }
        PostQuitMessage(0);
        break;
    }
    case WM_SOCKET_MSG:
    {
        std::string* pMsg = (std::string*)lParam;
        if (g_hLogEdit && pMsg)
        {
            int len = GetWindowTextLengthW(g_hLogEdit);
            SendMessageA(g_hLogEdit, EM_SETSEL, len, len);
            std::string logLine = *pMsg + "\r\n";
            std::wstring logWline = String2Wstring(logLine);
            SendMessageW(g_hLogEdit, EM_REPLACESEL, FALSE, (LPARAM)logWline.c_str());
        }
        delete pMsg;
        break;
    }

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

