#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <iostream>
#include <string>
#include <Shlwapi.h>
#include <strsafe.h>

#include "config.h"
#include "utils.h"

std::string
Wstring2String(std::wstring ws)
{
    if (ws.empty())
    {
        return std::string();
    }

    int sizeNeed = WideCharToMultiByte(CP_UTF8, 0, &ws[0], (int)ws.size(), NULL, 0,
                                       NULL, NULL);
    std::string s(sizeNeed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &ws[0], (int)ws.size(), &s[0], sizeNeed, NULL,
                        NULL);

    return s;
}

std::wstring
String2Wstring(std::string s)
{
    if (s.empty())
    {
        return std::wstring();
    }

    int sizeNeed = MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), NULL, 0);
    std::wstring ws(sizeNeed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), &ws[0], sizeNeed);

    return ws;
}

std::string
GetFileNameFromPath(const std::string& path)
{
    size_t pos = path.find_last_of("\\/");

    if (pos == std::string::npos)
    {
        return path;
    }

    return path.substr(pos + 1);
}

DWORD
GetProcessIdByName(const WCHAR* name)
{
    DWORD pid = -1;
    PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (Process32First(hSnapshot, &pe32))
    {
        do
        {
            if (_wcsicmp(pe32.szExeFile, name) == 0)
            {
                pid = pe32.th32ProcessID;
                break;
            }
        }
        while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);

    return pid;
}

HMODULE
GetTargetModuleBase(HANDLE process, std::string dll)
{
    DWORD cbNeeded;
    HMODULE moduleHandleList[512];
    BOOL ret = EnumProcessModulesEx(process, moduleHandleList,
                                    sizeof(moduleHandleList), &cbNeeded, LIST_MODULES_64BIT);

    if (!ret)
    {
        std::cerr << "EnumProcessModulesEx() failed\n";
        return NULL;
    }

    if (cbNeeded > sizeof(moduleHandleList))
    {
        return NULL;
    }

    DWORD processCount = cbNeeded / sizeof(HMODULE);

    char moduleName[32];

    for (DWORD i = 0; i < processCount; i++)
    {
        GetModuleBaseNameA(process, moduleHandleList[i], moduleName, 32);

        if (!strncmp(dll.c_str(), moduleName, dll.size()))
        {
            return moduleHandleList[i];
        }
    }

    return NULL;
}

static
int
GetWeChatPath(
    WCHAR* path
)
{
    int ret = -1;
    HKEY hKey = NULL;

    // HKEY_CURRENT_USER\Software\Tencent\WeChat InstallPath = xx
    if (ERROR_SUCCESS != RegOpenKey(HKEY_CURRENT_USER, L"Software\\Tencent\\WeChat",
                                    &hKey))
    {
        ret = GetLastError();
        return ret;
    }

    DWORD Type = REG_SZ;
    DWORD cbData = MAX_PATH * sizeof(WCHAR);

    if (ERROR_SUCCESS != RegQueryValueEx(hKey, L"InstallPath", 0, &Type,
                                         (LPBYTE)path, &cbData))
    {
        ret = GetLastError();
        goto __exit;
    }

    if (path != NULL)
    {
        PathAppend(path, WECHAT_EXE);
    }

__exit:

    if (hKey)
    {
        RegCloseKey(hKey);
    }

    return ERROR_SUCCESS;
}

static
int
GetWeChatWinDLLPath(
    WCHAR*
    path
)
{
    int ret = GetWeChatPath(path);

    if (ret != ERROR_SUCCESS)
    {
        return ret;
    }

    PathRemoveFileSpecW(path);
    PathAppendW(path, WECHAT_WIN_DLL);

    if (!PathFileExists(path))
    {
        // WeChatWin.dll may exist under xxx/[3.9.12.51]/
        PathRemoveFileSpec(path);
        _wfinddata_t findData;
        std::wstring dir = std::wstring(path) + L"\\[*.*";
        intptr_t handle = _wfindfirst(dir.c_str(), &findData);

        if (handle == -1)
        {
            return -1;
        }

        std::wstring dllPath = std::wstring(path) + L"\\" + findData.name;
        wcscpy_s(path, MAX_PATH, dllPath.c_str());
        PathAppend(path, WECHAT_WIN_DLL);
    }

    return ret;
}

static
bool
GetFileVersion(
    const WCHAR* filePath,
    WCHAR* version
)
{
    if (wcslen(filePath) > 0 && PathFileExists(filePath))
    {
        VS_FIXEDFILEINFO* pVerInfo = NULL;
        DWORD dwTemp, dwSize;
        BYTE* pData = NULL;
        UINT uLen;

        dwSize = GetFileVersionInfoSize(filePath, &dwTemp);

        if (dwSize == 0)
        {
            return false;
        }

        pData = new BYTE[dwSize + 1];

        if (pData == NULL)
        {
            return false;
        }

        if (!GetFileVersionInfo(filePath, 0, dwSize, pData))
        {
            delete[] pData;
            return false;
        }

        if (!VerQueryValue(pData, TEXT("\\"), (void * *)&pVerInfo, &uLen))
        {
            delete[] pData;
            return false;
        }

        UINT64 verMS = pVerInfo->dwFileVersionMS;
        UINT64 verLS = pVerInfo->dwFileVersionLS;
        UINT64 major = HIWORD(verMS);
        UINT64 minor = LOWORD(verMS);
        UINT64 build = HIWORD(verLS);
        UINT64 revision = LOWORD(verLS);
        delete[] pData;

        StringCbPrintf(version, 0x20, TEXT("%d.%d.%d.%d"), major, minor, build,
                       revision);

        return true;
    }

    return false;
}

int
GetWeChatVersion(
    WCHAR* version
)
{
    WCHAR Path[MAX_PATH] = { 0 };

    int ret = GetWeChatWinDLLPath(Path);

    if (ret != ERROR_SUCCESS)
    {
        return ret;
    }

    ret = GetFileVersion(Path, version);

    return ret;
}

BOOLEAN
IsWeChatSupported(
    CONST WCHAR* version
)
{
    if (wcscmp(version, SUPPORTED_WECHAT_VERSION) != 0)
    {
        return FALSE;
    }

    return TRUE;
}

UINT64
GetFuncOffsetInDll(
    LPCWSTR dllPath,
    LPCSTR funcName
)
{
    HMODULE dll = LoadLibrary(dllPath);

    if (dll == NULL)
    {
        return 0;
    }

    LPVOID absAddr = GetProcAddress(dll, funcName);
    UINT64 offset = (UINT64)absAddr - (UINT64)dll;
    FreeLibrary(dll);

    return offset;
}

BOOL
InjectSpyDll(
    DWORD pid,
    HANDLE* phWechat,
    const WCHAR* dllPath,
    HMODULE* phInjectDllBase
)
{
    // 1. Open target process
    HANDLE hWechat = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

    if (!hWechat)
    {
        return FALSE;
    }

    // 2. Write spy dll into target process
    LPVOID pRemotePath = VirtualAllocEx(hWechat, NULL, (wcslen(dllPath) + 1) * 2,
                                        MEM_COMMIT, PAGE_READWRITE);

    if (!pRemotePath)
    {
        CloseHandle(hWechat);
        *phWechat = NULL;
        return FALSE;
    }

    WriteProcessMemory(hWechat, pRemotePath, dllPath,
                       (wcslen(dllPath) + 1) * 2,
                       NULL);

    // 3. Load spy dll in target process
    LPTHREAD_START_ROUTINE pfnLoadLib = (LPTHREAD_START_ROUTINE)GetProcAddress(
                                            GetModuleHandle(L"kernel32.dll"), "LoadLibraryW");
    HANDLE hThread = CreateRemoteThread(hWechat, NULL, 0, pfnLoadLib, pRemotePath,
                                        0, NULL);

    if (!hThread)
    {
        VirtualFreeEx(hWechat, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hWechat);
        *phWechat = NULL;
        return FALSE;
    }

    // 4. Release resources before
    WaitForSingleObject(hThread, INFINITE);
    VirtualFreeEx(hWechat, pRemotePath, 0, MEM_RELEASE);
    CloseHandle(hThread);

    // 5. Get injected dll base
    *phInjectDllBase = GetTargetModuleBase(hWechat,
                                           GetFileNameFromPath(Wstring2String(dllPath)));
    *phWechat = hWechat;

    return TRUE;
}

BOOL
EjectSpyDll(
    DWORD pid,
    const WCHAR* dllName
)
{
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE,
                                  pid);

    if (!hProcess)
    {
        return FALSE;
    }

    HMODULE hModule = NULL;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);

    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        CloseHandle(hProcess);
        return FALSE;
    }

    MODULEENTRY32 me32 = { sizeof(MODULEENTRY32) };
    BOOL bFound = FALSE;

    if (Module32First(hSnapshot, &me32))
    {
        do
        {
            if (_wcsicmp(me32.szModule, dllName) == 0 ||
                    _wcsicmp(me32.szExePath, dllName) == 0)
            {
                hModule = me32.hModule;
                bFound = TRUE;
                break;
            }
        }
        while (Module32Next(hSnapshot, &me32));
    }

    CloseHandle(hSnapshot);

    if (!bFound)
    {
        CloseHandle(hProcess);
        return FALSE;
    }

    LPTHREAD_START_ROUTINE pfnFreeLib =
        (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle(L"kernel32.dll"),
            "FreeLibrary");

    if (!pfnFreeLib)
    {
        CloseHandle(hProcess);
        return FALSE;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, pfnFreeLib, hModule, 0,
                                        NULL);

    if (!hThread)
    {
        CloseHandle(hProcess);
        return FALSE;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    return TRUE;
}

BOOL
CallDllFuncEx(
    HANDLE process,
    LPCWSTR dllPath,
    HMODULE dllBase,
    LPCSTR funcName,
    LPVOID lpParameter,
    size_t szParameter,
    LPDWORD ret
)
{
    UINT64 offset = GetFuncOffsetInDll(dllPath, funcName);

    if (offset == 0)
    {
        return FALSE;
    }

    UINT64 pFunc = (UINT64)dllBase + GetFuncOffsetInDll(dllPath,
                   funcName);

    if (pFunc <= (UINT64)dllBase)
    {
        return FALSE;
    }

    LPVOID pRemoteAddress = NULL;

    if (szParameter)
    {
        pRemoteAddress = VirtualAllocEx(process, NULL, szParameter, MEM_COMMIT,
                                        PAGE_READWRITE);

        if (pRemoteAddress == NULL)
        {
            return FALSE;
        }

        WriteProcessMemory(process, pRemoteAddress, lpParameter,
                           szParameter, NULL);
    }

    HANDLE hThread = CreateRemoteThread(process, NULL, 0,
                                        (LPTHREAD_START_ROUTINE)pFunc, pRemoteAddress, (DWORD)szParameter, NULL);

    if (hThread == NULL)
    {
        if (pRemoteAddress)
        {
            VirtualFree(pRemoteAddress, 0, MEM_RELEASE);
        }

        return FALSE;
    }

    WaitForSingleObject(hThread, INFINITE);

    if (pRemoteAddress)
    {
        VirtualFree(pRemoteAddress, 0, MEM_RELEASE);
    }

    if (ret != NULL)
    {
        GetExitCodeThread(hThread, ret);
    }

    CloseHandle(hThread);

    return TRUE;
}