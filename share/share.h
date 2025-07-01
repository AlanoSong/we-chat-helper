#pragma once

#include <Windows.h>

typedef enum _FUNC_TYPE
{
    FUNC_TYPE_LOG_MSG = 0,
    FUNC_TYPE_SEND_MSG = 1,
    FUNC_TYPE_RECV_MSG = 2,

    FUNC_TOTAL_NUM = 3,
} FUNC_TYPE;

typedef struct _FUNC_SWITCH_LIST
{
    FUNC_TYPE funcType;
    BOOL tryHookThis;
} FUNC_ITEM;

typedef struct _SPY_CONTEXT
{
    FUNC_ITEM funcList[FUNC_TOTAL_NUM];
    ULONG funcCount;
    USHORT listenPort;
} SPY_CONTEXT, *PSPY_CONTEXT;