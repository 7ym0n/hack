// iocp.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <memory.h>
#include <winsock2.h>
#include <iostream>
#include <stdlib.h>
#pragma hdrstop

//---------------------------------------------------------------------------

#pragma comment(lib,"ws2_32.lib")

using namespace std;

#define RECV_POSTED 1001
#define SEND_POSTED 1002

int Init();

HANDLE hCompletionPort;
typedef struct _PER_HANDLE_DATA
{
	SOCKET sock;
}PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

typedef struct _PER_IO_OPERATION_DATA
{
	OVERLAPPED Overlapped;
	WSABUF DataBuff[1];
	char Buff[24];
	BOOL OperationType;
}PER_IO_OPERATION_DATA, *LPPER_IO_OPERATION_DATA;

DWORD WINAPI ServerWorkerThread(LPVOID CompletionPort);

int _tmain(int argc, _TCHAR* argv[])
{
	LPPER_HANDLE_DATA perHandleData;
	LPPER_IO_OPERATION_DATA ioperdata;
	SYSTEM_INFO siSys;
	SOCKET sockListen;
	struct sockaddr_in addrLocal;
	char buf[24];
	int nRet = 0;
	DWORD nThreadID;
	SOCKET sockAccept;
	DWORD dwFlags;
	DWORD dwRecvBytes;
	int nReuseAddr = 1;

	cout << "��ʼ����..." << endl;
	if (Init() != 0)
		goto theend;

	//����һ��IO��ɶ˿�
	cout << "����һ��IO��ɶ˿�" << endl;
	hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hCompletionPort == INVALID_HANDLE_VALUE)
	{
		cout << "����IO��ɶ˿�ʧ��" << endl;
		goto theend;
	}
	//��ȡCPU��Ŀ
	GetSystemInfo(&siSys);
	//����һ����Ŀ�Ĺ������̣߳���������һ��������һ���̴߳���
	for (int i = 0; i<(int)siSys.dwNumberOfProcessors * 2; i++)//NumberOfProcessors
	{
		HANDLE hThread;
		hThread = CreateThread(NULL, 0, ServerWorkerThread, (LPVOID)hCompletionPort, 0, &nThreadID);
		cout << "�����������߳�" << i << endl;
		CloseHandle(hThread);
	}
	//��������SOCKET
	cout << "��������SOCKET" << endl;
	sockListen = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (sockListen == SOCKET_ERROR)
	{
		cout << "WSASocket����" << endl;
		goto theend;
	}

	if (setsockopt(sockListen, SOL_SOCKET, SO_REUSEADDR, (const char *)&nReuseAddr, sizeof(int)) != 0)
	{
		cout << "setsockopt����" << endl;
		goto theend;
	}
	addrLocal.sin_family = AF_INET;
	addrLocal.sin_addr.s_addr = htonl(INADDR_ANY);
	addrLocal.sin_port = htons(9090);
	if (bind(sockListen, (struct sockaddr *)&addrLocal, sizeof(sockaddr_in)) != 0)
	{
		cout << "bind����" << endl;
		int n = WSAGetLastError();
		goto theend;
	}
	//׼������
	cout << "׼������" << endl;
	if (listen(sockListen, 5) != 0)
	{
		cout << "listen����" << endl;
		goto theend;
	}
	while (true)
	{
		//�����û����ӣ�������ɶ˿ڹ���
		sockAccept = WSAAccept(sockListen, NULL, NULL, NULL, 0);
		perHandleData = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
		if (perHandleData == NULL)
			continue;
		cout << "socket number " << sockAccept << "����" << endl;
		perHandleData->sock = sockAccept;

		ioperdata = (LPPER_IO_OPERATION_DATA)malloc(sizeof(PER_IO_OPERATION_DATA));
		memset(&(ioperdata->Overlapped), 0, sizeof(OVERLAPPED));
		(ioperdata->DataBuff[0]).len = 24;
		(ioperdata->DataBuff[0]).buf = ioperdata->Buff;
		ioperdata->OperationType = RECV_POSTED;
		if (ioperdata == NULL)
		{
			free(perHandleData);
			continue;
		}
		//����
		cout << "����SOCKET����ɶ˿�" << endl;
		if (CreateIoCompletionPort((HANDLE)sockAccept, hCompletionPort, (DWORD)perHandleData, 1) == NULL)
		{
			cout << sockAccept << "createiocompletionport����" << endl;
			free(perHandleData);
			free(ioperdata);
			continue;
		}
		//Ͷ�ݽ��ղ���
		cout << "Ͷ�ݽ��ղ���" << endl;
		WSARecv(perHandleData->sock, ioperdata->DataBuff, 1, &dwRecvBytes, &dwFlags, &(ioperdata->Overlapped), NULL);
	}
theend:
	getchar();
	return 0;
}
//---------------------------------------------------------------------------
int Init()
{
	WSAData wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "WSAStartupʧ��" << endl;
		return -1;
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		cout << "SOCKET�汾����" << endl;
		WSACleanup();
		return -1;
	}
	return 0;
}

DWORD WINAPI ServerWorkerThread(LPVOID CompletionPort)
{
	HANDLE ComPort = (HANDLE)CompletionPort;
	DWORD BytesTransferred;
	LPOVERLAPPED Overlapped;
	LPPER_HANDLE_DATA PerHandleData;
	LPPER_IO_OPERATION_DATA PerIoData;
	DWORD SendBytes, RecvBytes;
	DWORD Flags;
	BOOL bT;

	while (TRUE)
	{
		//�ȴ���ɶ˿���SOCKET�����
		cout << "�ȴ���ɶ˿���SOCKET�����" << endl;
		bT = GetQueuedCompletionStatus(ComPort,
			&BytesTransferred, (LPDWORD)&PerHandleData,
			(LPOVERLAPPED *)&PerIoData, INFINITE);

		//����Ƿ��д������
		if (BytesTransferred == 0 &&
			(PerIoData->OperationType == RECV_POSTED ||
			PerIoData->OperationType == SEND_POSTED))
		{
			//�ر�SOCKET
			cout << PerHandleData->sock << "SOCKET�ر�" << endl;
			closesocket(PerHandleData->sock);
			free(PerHandleData);
			free(PerIoData);
			continue;
		}

		//Ϊ�������
		cout << "wocaole..." << endl;
		if (PerIoData->OperationType == RECV_POSTED)
		{
			//����
			cout << "���մ���" << endl;
			cout << PerHandleData->sock << "SOCKET :" << PerIoData->Buff << endl;
			//��Ӧ�ͻ���
			ZeroMemory(PerIoData->Buff, 24);
			strcpy_s(PerIoData->Buff, "OK");
			Flags = 0;
			ZeroMemory((LPVOID)&(PerIoData->Overlapped), sizeof(OVERLAPPED));
			PerIoData->DataBuff[0].len = 2;
			PerIoData->DataBuff[0].buf = PerIoData->Buff;
			PerIoData->OperationType = SEND_POSTED;
			WSASend(PerHandleData->sock, PerIoData->DataBuff,
				1, &SendBytes, 0, &(PerIoData->Overlapped), NULL);
		}
		else //if(PerIoData->OperationType == SEND_POSTED)
		{
			//����ʱ�Ĵ���
			cout << "���ʹ���" << endl;
			Flags = 0;
			ZeroMemory((LPVOID)&(PerIoData->Overlapped), sizeof(OVERLAPPED));
			ZeroMemory(PerIoData->Buff, 24);
			PerIoData->DataBuff[0].len = 24;
			PerIoData->DataBuff[0].buf = PerIoData->Buff;
			PerIoData->OperationType = RECV_POSTED;
			WSARecv(PerHandleData->sock, PerIoData->DataBuff,
				1, &RecvBytes, &Flags, &(PerIoData->Overlapped), NULL);
		}
	}
}


