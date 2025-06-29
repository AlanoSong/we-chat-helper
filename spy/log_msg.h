#pragma once

#include "common.h"

extern SOCKET g_sendSock;

const unsigned long long LOG_MSG_OFFSET = 0x0000000002627590;

typedef char* (__fastcall* PFN_LOG_MSG)(
    __int64 a1,
    __int64 a2,
    int a3,
    __int64 a4,
    const char* a5,
    const char* a6,
    fake_int128* a7,
    fake_int128* a8,
    fake_int128* a9,
    fake_int128* a10,
    fake_int128* a11,
    fake_int128* a12);

static PFN_LOG_MSG pOriginalLogMsg = NULL;

char* __fastcall hookLogMsg(
    __int64 a1,
    __int64 a2,
    int a3,
    __int64 a4,
    const char* a5,
    const char* a6,
    fake_int128* a7,
    fake_int128* a8,
    fake_int128* a9,
    fake_int128* a10,
    fake_int128* a11,
    fake_int128* a12)
{
    char* pOriLogMsg = pOriginalLogMsg(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12);

    try
    {
        if (pOriLogMsg == NULL || (ULONGLONG)pOriLogMsg == 0x1)
        {
            return pOriLogMsg;
        }

        size_t len = strlen(pOriLogMsg);
        char pLogMsg[0x1000] = { 0 };
        strncpy_s(pLogMsg, pOriLogMsg, len < 0x1000 ? len : 0x1000);
        std::string logStr(pLogMsg);
        std::wstring logWstr = String2Wstring(logStr);
        OutputDebugByUdpW(logWstr.c_str());

        return pOriLogMsg;
    }
    catch (...)
    {
        OutputDebugByUdpW(
            L"[spy] - Exception occurred in hookLogMsg\n");
        return NULL;
    }
}