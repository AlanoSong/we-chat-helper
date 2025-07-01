#pragma once
#include "common.h"

const unsigned long long RECV_MSG_OFFSET =
    0x000000000214C6C0;

#define RECV_MSG_ID_OFFSET          0x30
#define RECV_MSG_TYPE_OFFSET        0x38
#define RECV_MSG_SELF_OFFSET        0x3C
#define RECV_MSG_TS_OFFSET          0x44
#define RECV_MSG_ROOMID_OFFSET      0x48
#define RECV_MSG_CONTENT_OFFSET     0x88
#define RECV_MSG_WXID_OFFSET        0x240
#define RECV_MSG_SIGN_OFFSET        0x260
#define RECV_MSG_THUMB_OFFSET       0x280
#define RECV_MSG_EXTRA_OFFSET       0x2A0
#define RECV_MSG_XML_OFFSET         0x308
#define RECV_MSG_CALL_OFFSET        0x213ED90

typedef __int64(__fastcall* PFN_RECV_MSG)(
    __int64 a1,
    __int64 a2);

static PFN_RECV_MSG pOriRecvMsg = NULL;

typedef enum _WX_MSG_TYPE
{
    TYPE_MOMENTS    = 0x00000000,
    TYPE_TXT        = 0x00000001,
    TYPE_PICTURE    = 0x00000003,
    TYPE_VOICE      = 0x00000022,
    TYPE_ADD_FRIEND = 0x00000025,
    TYPE_VIDEO      = 0x0000002B,
    TYPE_POSITION   = 0x00000030,
    TYPE_REVOKE     = 0x00002712,
    TYPE_FILE       = 0x41000031,
} WX_MSG_TYPE;

inline
LPCWSTR
MsgTypeToStr(
    WX_MSG_TYPE type
)
{
    switch (type)
    {
    case TYPE_MOMENTS: return L"moments";
    case TYPE_TXT: return L"text";
    case TYPE_PICTURE: return L"picture";
    case TYPE_ADD_FRIEND: return L"add-friend";
    case TYPE_VIDEO: return L"video";
    case TYPE_POSITION: return L"position";
    case TYPE_REVOKE: return L"revoke";
    case TYPE_FILE: return L"file";
    default: return L"unknown";
    }
}

typedef struct
{
    BOOL isFromSelf;
    BOOL isGroupMsg;
    ULONG msgType;
    ULONG ts;
    ULONGLONG msgId;
    std::wstring sender;
    std::wstring roomId;
    std::wstring content;
    std::wstring sign;
    std::wstring thumb;
    std::wstring extra;
    std::wstring xml;
} RECV_MSG;

__int64 __fastcall
hookRecvMsg(
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
        recvMsg.msgId       = GET_QWORD(a2 + RECV_MSG_ID_OFFSET);
        recvMsg.msgType     = GET_DWORD(a2 + RECV_MSG_TYPE_OFFSET);
        recvMsg.isFromSelf  = GET_DWORD(a2 + RECV_MSG_SELF_OFFSET);
        recvMsg.ts          = GET_DWORD(a2 + RECV_MSG_TS_OFFSET);
        recvMsg.content     = GET_WSTRING(a2 + RECV_MSG_CONTENT_OFFSET);
        recvMsg.sign        = GET_WSTRING(a2 + RECV_MSG_SIGN_OFFSET);
        recvMsg.xml         = GET_WSTRING(a2 + RECV_MSG_XML_OFFSET);
        recvMsg.roomId      = GET_WSTRING(a2 + RECV_MSG_ROOMID_OFFSET);

        SendInfoToUdpSrvW(
            L"[spy] - [recv] - "
         "[msg id]/[0x%x] - "
         "[type]/[%ws] - "
         "[self]/[%d] - "
         "[room id]/[%ws] - "
         "[content]/[%ws]\n",
            recvMsg.msgId,
            MsgTypeToStr((WX_MSG_TYPE)recvMsg.msgType),
            recvMsg.isFromSelf,
            recvMsg.roomId.c_str(),
            recvMsg.content.c_str());

        return pOriRet;

    }

    catch (...)
    {
        SendInfoToUdpSrvW(
            L"[spy] - Exception occurred in hookRecvMsg\n");
        return NULL;
    }
}