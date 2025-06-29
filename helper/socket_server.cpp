#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>

#include "socket_server.h"

extern SOCKET g_recvSock;

DWORD WINAPI SocketServerThread(LPVOID lpParam)
{
    static SOCKET_SERVER_INFO srvInfo = *(SOCKET_SERVER_INFO*)lpParam;
    HWND hwnd = srvInfo.hWindow;
    WSADATA wsaData;
    sockaddr_in serverAddr = {};

    int len = sizeof(SOCKADDR_IN);
    sockaddr_in fromAddr;
    char recvBuf[4096] = { 0 };

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        return 1;
    }

    SOCKET udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSock == INVALID_SOCKET)
    {
        goto _CLEANUP;
    }
    else
    {
        g_recvSock = udpSock;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddr.sin_port = htons(srvInfo.listenPort);

    if (bind(udpSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        goto _CLEANUP;
    }

    while (true)
    {
        int recvLen = recvfrom(udpSock, recvBuf, ARRAYSIZE(recvBuf) - 1, 0, (sockaddr*)&fromAddr, &len);
        if (recvLen < 0)
        {
            break;
        }

        if (recvLen > 0)
        {
            recvBuf[recvLen] = '\0';
            PostMessage(hwnd, srvInfo.msgId, 0, (LPARAM)new std::string(recvBuf));
        }
    }

_CLEANUP:
    if (udpSock != INVALID_SOCKET)
    {
        closesocket(udpSock);
        udpSock = INVALID_SOCKET;
    }

    WSACleanup();
    return 0;
}