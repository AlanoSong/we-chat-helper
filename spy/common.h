#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>

sockaddr_in g_serverAddr = { 0 };
SOCKET g_sendSock = INVALID_SOCKET;

#define GET_UINT64(addr)         ((UINT64) * (UINT64 *)(addr))
#define GET_DWORD(addr)          ((DWORD) * (UINT64 *)(addr))
#define GET_QWORD(addr)          ((UINT64) * (UINT64 *)(addr))
#define GET_STRING(addr)         ((CHAR *)(*(UINT64 *)(addr)))
#define GET_WSTRING(addr)        ((WCHAR *)(*(UINT64 *)(addr)))

typedef struct { unsigned char data[16]; } fake_int128;

std::string Wstring2String(std::wstring ws)
{
    if (ws.empty())
    {
        return std::string();
    }
    int sizeNeed = WideCharToMultiByte(CP_UTF8, 0, &ws[0], (int)ws.size(), NULL, 0, NULL, NULL);
    std::string s(sizeNeed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &ws[0], (int)ws.size(), &s[0], sizeNeed, NULL, NULL);

    return s;
}

std::wstring String2Wstring(std::string s)
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

inline void OutputDebugByTcp(SOCKET sock, const char* strOutputString, ...)
{
    if (sock == INVALID_SOCKET)
    {
        return;
    }

    char strBuffer[4096] = { 0 };
    va_list vlArgs;

    va_start(vlArgs, strOutputString);
    _vsnprintf_s(strBuffer, sizeof(strBuffer) - 1, strOutputString, vlArgs);
    va_end(vlArgs);

    int sent = send(sock, strBuffer, (int)strlen(strBuffer), 0);
}

inline void OutputDebugByTcpW(SOCKET sock, const wchar_t* strOutputString, ...)
{
    if (sock == INVALID_SOCKET)
    {
        return;
    }

    wchar_t wstrBuffer[4096] = { 0 };
    va_list vlArgs;

    va_start(vlArgs, strOutputString);
    _vsnwprintf_s(wstrBuffer, sizeof(wstrBuffer) / sizeof(wchar_t), _TRUNCATE, strOutputString, vlArgs);
    va_end(vlArgs);

    // Convert wide string to UTF-8 for transmission
    char strBuffer[4096] = { 0 };
    int len = WideCharToMultiByte(CP_UTF8, 0, wstrBuffer, -1, strBuffer, sizeof(strBuffer) - 1, NULL, NULL);
    if (len > 0)
    {
        send(sock, strBuffer, (int)strlen(strBuffer), 0);
    }
}

inline void OutputDebugByUdpW(const wchar_t* strOutputString, ...)
{
    if (g_sendSock == INVALID_SOCKET)
    {
        return;
    }

    wchar_t wstrBuffer[4096] = { 0 };
    va_list vlArgs;

    va_start(vlArgs, strOutputString);
    _vsnwprintf_s(wstrBuffer, sizeof(wstrBuffer) / sizeof(wchar_t), _TRUNCATE, strOutputString, vlArgs);
    va_end(vlArgs);

    // Convert wide string to UTF-8 for transmission
    char strBuffer[4096] = { 0 };
    int len = WideCharToMultiByte(CP_UTF8, 0, wstrBuffer, -1, strBuffer, sizeof(strBuffer) - 1, NULL, NULL);
    if (len > 0)
    {
        sendto(g_sendSock, strBuffer, (int)strlen(strBuffer), 0, (sockaddr*)&g_serverAddr, sizeof(sockaddr_in));
    }
}