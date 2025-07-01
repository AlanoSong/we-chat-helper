#pragma once

#define SOCKET_SERVER_PORT 55555

typedef struct _SOCKET_SERVER_INFO
{
    HWND hWindow;
    ULONG msgId;
    USHORT listenPort;
} SOCKET_SERVER_INFO;

DWORD WINAPI SocketServerThread(LPVOID lpParam);