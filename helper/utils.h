#pragma once

std::string Wstring2String(std::wstring ws);
std::wstring String2Wstring(std::string s);

std::string GetFileNameFromPath(const std::string& path);
DWORD GetProcessIdByName(const WCHAR* name);
HMODULE GetTargetModuleBase(HANDLE process,
                            std::string dll);

UINT64 GetFuncOffsetInDll(LPCWSTR dllPath, LPCSTR funcName);

BOOL
InjectSpyDll(
    DWORD pid,
    HANDLE* phWechat,
    const WCHAR* dllPath,
    HMODULE* phInjectDllBase
);

BOOL
EjectSpyDll(
    DWORD pid,
    const WCHAR* dllName
);

BOOL CallDllFuncEx(
    HANDLE process,
    LPCWSTR dllPath,
    HMODULE dllBase,
    LPCSTR funcName,
    LPVOID parameter,
    size_t sz,
    LPDWORD ret
);

int
GetWeChatVersion(
    WCHAR* version
);

BOOLEAN
IsWeChatSupported(
    CONST WCHAR* version
);