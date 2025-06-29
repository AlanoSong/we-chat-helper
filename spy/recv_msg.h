#pragma once
#include "common.h"

extern SOCKET g_sendSock;

const unsigned long long RECV_MSG_OFFSET = 0x000000000214C6C0;

#define RECV_MSG_ID      0x30
#define RECV_MSG_TYPE    0x38
#define RECV_MSG_SELF    0x3C
#define RECV_MSG_TS      0x44
#define RECV_MSG_ROOMID  0x48
#define RECV_MSG_CONTENT 0x88
#define RECV_MSG_WXID    0x240
#define RECV_MSG_SIGN    0x260
#define RECV_MSG_THUMB   0x280
#define RECV_MSG_EXTRA   0x2A0
#define RECV_MSG_XML     0x308
#define RECV_MSG_CALL    0x213ED90

typedef __int64(__fastcall* PFN_RECV_MSG)(
    __int64 a1,
    __int64 a2);

static PFN_RECV_MSG pOriRecvMsg = NULL;

typedef struct
{
    ULONG type;
    ULONG ts;
    ULONGLONG id;
    std::wstring sender;
    std::wstring roomid;
    std::wstring content;
    std::wstring sign;
    std::wstring thumb;
    std::wstring extra;
    std::wstring xml;
} RECV_MSG;

__int64 __fastcall hookRecvMsg(
    __int64 a1,
    __int64 a2
)
{
    __int64 pOriRet = 0;
    if (pOriRecvMsg)
    {
        pOriRet = pOriRecvMsg(a1, a2);
    }

    try
    {
        RECV_MSG recvMsg = { 0 };
        recvMsg.id      = GET_QWORD(a2 + RECV_MSG_ID);
        recvMsg.type    = GET_DWORD(a2 + RECV_MSG_TYPE);
        recvMsg.ts      = GET_DWORD(a2 + RECV_MSG_TS);
        recvMsg.content = GET_WSTRING(a2 + RECV_MSG_CONTENT);
        recvMsg.sign    = GET_WSTRING(a2 + RECV_MSG_SIGN);
        recvMsg.xml     = GET_WSTRING(a2 + RECV_MSG_XML);

        OutputDebugByUdpW(
            L"[spy] - [recv]: %s\n",
            recvMsg.content.c_str());

        return pOriRet;
    }
    catch (...)
    {
        OutputDebugByUdpW(
            L"[spy] - Exception occurred in hookRecvMsg\n");
        return NULL;
    }
}