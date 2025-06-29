#include "pch.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>

#include "..\share\share.h"
#include "..\3rdparty\minhook\include\MinHook.h"

#include "log_msg.h"
#include "send_msg.h"
#include "recv_msg.h"

static BOOL isMinHookStarted = FALSE;

static VOID CreateUdpConnection(USHORT listenPort)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        return;
    }

    g_sendSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    g_serverAddr.sin_family = AF_INET;
    g_serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
    g_serverAddr.sin_port = htons(listenPort);
}

static VOID CloseUdpConnection(VOID)
{
    if (g_sendSock != INVALID_SOCKET)
    {
        closesocket(g_sendSock);
        WSACleanup();
    }
}

extern "C"
__declspec(dllexport)
void
InstallHook(
    LPVOID args
)
{
    PSPY_CONTEXT pSpyCtx = (PSPY_CONTEXT)args;
    // Connect udp socket to output debug
    CreateUdpConnection(pSpyCtx->listenPort);

    OutputDebugByUdpW(
        L"[spy] - Install hook\n");

    if (!isMinHookStarted)
    {
        if (MH_Initialize() == MH_OK)
        {
            HMODULE hModule = GetModuleHandleA("WeChatWin.dll");

            MH_STATUS status = MH_OK;

            for (ULONG idx = 0; idx < pSpyCtx->funcCount; idx++)
            {
                if (pSpyCtx->funcList[idx].tryHookThis)
                {
                    switch (pSpyCtx->funcList[idx].funcType)
                    {
                    case FUNC_TYPE_LOG_MSG:
                    {
                        LPVOID pTargetFunc = (PFN_LOG_MSG)(
                            (unsigned long long)hModule + LOG_MSG_OFFSET);
                        status = MH_CreateHook(pTargetFunc,
                            &hookLogMsg,
                            (LPVOID*)&pOriginalLogMsg);
                        OutputDebugByUdpW(
                            L"[spy] - Hook log msg @ 0x%p\n",
                            pTargetFunc);

                        if (status == MH_OK)
                        {
                            status = MH_EnableHook(pTargetFunc);
                            OutputDebugByUdpW(
                                L"[spy] - Enable log msg status: 0x%x\n",
                                status);

                            isMinHookStarted = TRUE;
                        }
                        break;
                    }
                    case FUNC_TYPE_SEND_MSG:
                    {
                        LPVOID pTargetFunc = (PFN_SEND_MSG)(
                            (unsigned long long)hModule + SEND_MSG_OFFSET);
                        status = MH_CreateHook(pTargetFunc,
                            &hookSendMsg,
                            (LPVOID*)&pOriSendMsg);

                        OutputDebugByUdpW(
                            L"[spy] - Hook send msg @ 0x%p\n",
                            pTargetFunc);

                        if (status == MH_OK)
                        {
                            status = MH_EnableHook(pTargetFunc);
                            OutputDebugByUdpW(
                                L"[spy] - Enable send msg status: 0x%x\n",
                                status);
                            isMinHookStarted = TRUE;
                        }
                        break;
                    }
                    case FUNC_TYPE_RECV_MSG:
                    {
                        LPVOID pTargetFunc = (PFN_SEND_MSG)(
                            (unsigned long long)hModule + RECV_MSG_OFFSET);
                        status = MH_CreateHook(pTargetFunc,
                            &hookRecvMsg,
                            (LPVOID*)&pOriRecvMsg);

                        OutputDebugByUdpW(
                            L"[spy] - Hook recv msg @ 0x%p\n",
                            pTargetFunc);

                        if (status == MH_OK)
                        {
                            status = MH_EnableHook(pTargetFunc);
                            OutputDebugByUdpW(
                                L"[spy] - Enable recv msg status: 0x%x\n",
                                status);
                            isMinHookStarted = TRUE;
                        }
                        break;
                    }
                    default:
                        OutputDebugByUdpW(
                            L"[spy] - Unknown func type\n");
                        break;
                    }
                }
            }
        }
    }
}

extern "C"
__declspec(dllexport)
void
RemoveHook()
{
    OutputDebugByUdpW(
        L"[spy] - Remove hook\n");

    if (isMinHookStarted)
    {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();

        pOriginalLogMsg = NULL;
        pOriSendMsg = NULL;

        isMinHookStarted = FALSE;
    }

    CloseUdpConnection();
}

BOOL
APIENTRY
DllMain(
    HMODULE hModule,
    DWORD  reason,
    LPVOID lpReserved
)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}

