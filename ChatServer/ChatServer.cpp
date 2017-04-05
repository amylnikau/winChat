#include "stdafx.h"
#include "ChatServer.h"

int main()
{

	if (false == Initialize())
	{
		return 1;
	}

	SOCKET ListenSocket;

	struct sockaddr_in ServerAddress;

	//Overlapped I/O follows the model established in Windows and can be performed only on 
	//sockets created through the WSASocket function 
	ListenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (INVALID_SOCKET == ListenSocket)
	{
		printf("\nError occurred while opening socket: %d.", WSAGetLastError());
		goto error;
	}
	else
	{
		printf("\nWSASocket() successful.");
	}

	//Cleanup and Init with 0 the ServerAddress
	ZeroMemory((char *)&ServerAddress, sizeof(ServerAddress));

	//Fill up the address structure
	ServerAddress.sin_family = AF_INET;
	ServerAddress.sin_addr.s_addr = INADDR_ANY; //WinSock will supply address
	ServerAddress.sin_port = htons(DEFAULT_PORT);    //comes from commandline

	//Assign local address and port number
	if (SOCKET_ERROR == bind(ListenSocket, (struct sockaddr *) &ServerAddress, sizeof(ServerAddress)))
	{
		closesocket(ListenSocket);
		printf("\nError occurred while binding.");
		goto error;
	}
	else
	{
		printf("\nbind() successful.");
	}

	//Make the socket a listening socket
	if (SOCKET_ERROR == listen(ListenSocket, SOMAXCONN))
	{
		closesocket(ListenSocket);
		printf("\nError occurred while listening.");
		goto error;
	}
	else
	{
		printf("\nlisten() successful.");
	}

	g_hAcceptEvent = WSACreateEvent();

	if (WSA_INVALID_EVENT == g_hAcceptEvent)
	{
		printf("\nError occurred while WSACreateEvent().");
		goto error;
	}

	if (SOCKET_ERROR == WSAEventSelect(ListenSocket, g_hAcceptEvent, FD_ACCEPT))
	{
		printf("\nError occurred while WSAEventSelect().");
		WSACloseEvent(g_hAcceptEvent);
		goto error;
	}

	printf("\nTo exit this server, hit a key at any time on this console...");

	DWORD nThreadID;
	g_hAcceptThread = CreateThread(0, 0, AcceptThread, (void *)ListenSocket, 0, &nThreadID);

	//Hang in there till a key is hit
	_getch();

	WriteToConsole("\nServer is shutting down...");

	//Start cleanup
	CleanUp();

	//Close open sockets
	closesocket(ListenSocket);

	DeInitialize();
	return 0; //success

error:
	closesocket(ListenSocket);
	DeInitialize();
	return 1;
}

bool Initialize()
{
	//Find out number of processors and threads
	g_nThreads = WORKER_THREADS_PER_PROCESSOR * GetNoOfProcessors();

	printf("\nNumber of processors on host: %d", GetNoOfProcessors());

	printf("\nThe following number of worker threads will be created: %d", g_nThreads);

	//Allocate memory to store thread handless
	g_phWorkerThreads = new HANDLE[g_nThreads];

	//Initialize the Console Critical Section
	InitializeCriticalSection(&g_csConsole);

	//Initialize the Client List Critical Section
	InitializeCriticalSection(&g_csClientList);

	//Create shutdown event
	g_hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	// Initialize Winsock
	WSADATA wsaData;

	int nResult;
	nResult = WSAStartup(WINSOCK_VERSION, &wsaData);

	if (NO_ERROR != nResult)
	{
		printf("\nError occurred while executing WSAStartup().");
		return false; //error
	}
	else
	{
		printf("\nWSAStartup() successful.");
	}

	if (false == InitializeIOCP())
	{
		printf("\nError occurred while initializing IOCP");
		return false;
	}
	else
	{
		printf("\nIOCP initialization successful.");
	}

	return true;
}

//Function to Initialize IOCP
bool InitializeIOCP()
{
	//Create I/O completion port
	g_hIOCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	if (NULL == g_hIOCompletionPort)
	{
		printf("\nError occurred while creating IOCP: %d.", WSAGetLastError());
		return false;
	}

	DWORD nThreadID;

	//Create worker threads
	for (int ii = 0; ii < g_nThreads; ii++)
	{
		g_phWorkerThreads[ii] = CreateThread(0, 0, WorkerThread, (void *)(ii + 1), 0, &nThreadID);
	}

	return true;
}

void CleanUp()
{
	//Ask all threads to start shutting down
	SetEvent(g_hShutdownEvent);

	//Let Accept thread go down
	WaitForSingleObject(g_hAcceptThread, INFINITE);

	for (int i = 0; i < g_nThreads; i++)
	{
		//Help threads get out of blocking - GetQueuedCompletionStatus()
		PostQueuedCompletionStatus(g_hIOCompletionPort, 0, (DWORD)NULL, NULL);
	}

	//Let Worker Threads shutdown
	WaitForMultipleObjects(g_nThreads, g_phWorkerThreads, TRUE, INFINITE);

	//We are done with this event
	WSACloseEvent(g_hAcceptEvent);

	//Cleanup dynamic memory allocations, if there are any.
	CleanClientList();
}

void DeInitialize()
{
	//Delete the Console Critical Section.
	DeleteCriticalSection(&g_csConsole);

	//Delete the Client List Critical Section.
	DeleteCriticalSection(&g_csClientList);

	//Cleanup IOCP.
	CloseHandle(g_hIOCompletionPort);

	//Clean up the event.
	CloseHandle(g_hShutdownEvent);

	//Clean up memory allocated for the storage of thread handles
	delete[] g_phWorkerThreads;

	//Cleanup Winsock
	WSACleanup();
}

//This thread will look for accept event
DWORD WINAPI AcceptThread(LPVOID lParam)
{
	SOCKET ListenSocket = (SOCKET)lParam;

	WSANETWORKEVENTS WSAEvents;
	HANDLE events[2] = { g_hAcceptEvent, g_hShutdownEvent };
	//Accept thread will be around to look for accept event, until a Shutdown event is not Signaled.
	while (TRUE)
	{
		switch (WaitForMultipleObjects(2, events, FALSE, INFINITE))
		{
		case WAIT_OBJECT_0:
			WSAEnumNetworkEvents(ListenSocket, g_hAcceptEvent, &WSAEvents);
			if ((WSAEvents.lNetworkEvents & FD_ACCEPT) && (0 == WSAEvents.iErrorCode[FD_ACCEPT_BIT]))
			{
				//Process it
				AcceptConnection(ListenSocket);
			}
			break;
		case WAIT_OBJECT_0 + 1:
			return 0;
		}
	}

	return 0;
}

//This function will process the accept event
void AcceptConnection(SOCKET ListenSocket)
{
	sockaddr_in ClientAddress;
	int nClientLength = sizeof(ClientAddress);

	//Accept remote connection attempt from the client
	SOCKET Socket = accept(ListenSocket, (sockaddr*)&ClientAddress, &nClientLength);

	if (INVALID_SOCKET == Socket)
	{
		WriteToConsole("\nError occurred while accepting socket: %ld.", WSAGetLastError());
	}
	char buffer[16];
	InetNtopA(AF_INET, &ClientAddress.sin_addr, buffer, 16);
	//Display Client's IP
	WriteToConsole("\nClient connected from: %s", buffer);

	//Create a new ClientContext for this newly accepted client
	CClientContext   *pClientContext = new CClientContext;

	pClientContext->SetSocket(Socket);

	//Store this object
	AddToClientList(pClientContext);

	if (AssociateWithIOCP(pClientContext))
	{

		WSABUF *p_wbuf = pClientContext->GetWSABUFPtr();
		OVERLAPPED *p_ol = pClientContext->GetOVERLAPPEDPtr();

		//Get data.
		DWORD dwFlags = 0;
		DWORD dwBytes = 0;

		int nBytesRecv = WSARecv(pClientContext->GetSocket(), p_wbuf, 1,
			&dwBytes, &dwFlags, p_ol, NULL);

		if ((SOCKET_ERROR == nBytesRecv) && (WSA_IO_PENDING != WSAGetLastError()))
		{
			WriteToConsole("\nError in Initial Post.");
		}
	}
}

bool AssociateWithIOCP(CClientContext  *pClientContext)
{
	//Associate the socket with IOCP
	HANDLE hTemp = CreateIoCompletionPort((HANDLE)pClientContext->GetSocket(), g_hIOCompletionPort, (DWORD)pClientContext, 0);

	if (NULL == hTemp)
	{
		WriteToConsole("\nError occurred while executing CreateIoCompletionPort().");

		//Let's not work with this client
		RemoveFromClientListAndFreeMemory(pClientContext);

		return false;
	}

	return true;
}

//Worker thread will service IOCP requests
DWORD WINAPI WorkerThread(LPVOID lpParam)
{
	int nThreadNo = (int)lpParam;

	void *lpContext = NULL;
	OVERLAPPED       *pOverlapped = NULL;
	CClientContext   *pClientContext = NULL;
	CClientContext   *pSendClientContext = NULL;
	DWORD            dwBytesTransfered = 0;
	int nBytesRecv = 0;
	int nBytesSent = 0;
	DWORD             dwBytes = 0, dwFlags = 0;

	//Worker thread will be around to process requests, until a Shutdown event is not Signaled.
	while (TRUE)
	{
		BOOL bReturn = GetQueuedCompletionStatus(
			g_hIOCompletionPort,
			&dwBytesTransfered,
			(LPDWORD)&lpContext,
			&pOverlapped,
			INFINITE);

		if (NULL == lpContext)
		{
			//We are shutting down
			break;
		}

		//Get the client context
		pClientContext = (CClientContext *)lpContext;
		if ((FALSE == bReturn) || ((TRUE == bReturn) && (0 == dwBytesTransfered)))
		{
			//Client connection gone, remove it.
			RemoveFromClientListAndFreeMemory(pClientContext);
			continue;
		}

		WSABUF *p_wbuf = pClientContext->GetWSABUFPtr();
		OVERLAPPED *p_ol = pClientContext->GetOVERLAPPEDPtr();
		ChatMSG msg = pClientContext->GetMSG();
		ChatMSG sendMsg;
		char buffer[BUFSIZE];
		int id;
		switch (msg.type)
		{
		case MT_LOGIN:
			pClientContext->SetClientName(msg.message);
			break;
		case MT_NEW_ROOM:
			id = AddRoom(msg.message, pClientContext);
			sendMsg.type = MT_NEW_ROOM;
			sendMsg.room_id = id;
			sendMsg.status = MS_NODATA;
			pClientContext->setMSG(sendMsg);
			send(pClientContext->GetSocket(), pClientContext->GetSendBuffer(), PACKETSIZE, NULL);
			break;
		case MT_LIST_USER:
			EnterCriticalSection(&g_csClientList);
			sendMsg.type = MT_LIST_USER;
			sendMsg.status = MS_OK;
			for (size_t i = 0;i < g_ClientContext.size();++i) {
				if (g_ClientContext[i] == pClientContext)
					continue;
				g_ClientContext[i]->GetClientName(sendMsg.message);
				if (i == g_ClientContext.size() - 1 || (i == g_ClientContext.size() - 2 && g_ClientContext[i + 1] == pClientContext))
					sendMsg.status = MS_LAST;
				pClientContext->setMSG(sendMsg);
				send(pClientContext->GetSocket(), pClientContext->GetSendBuffer(), PACKETSIZE, NULL);
			}
			LeaveCriticalSection(&g_csClientList);
			break;
		case MT_JOIN_USER:
			EnterCriticalSection(&g_csClientList);
			pSendClientContext = JoinClient(msg.room_id, msg.message);
			if (pSendClientContext) {
				sendMsg.type = MT_SEND_MESSAGE;
				sendMsg.room_id = msg.room_id;
				sendMsg.status = MS_OK;
				for (const auto client : g_RoomClient[msg.room_id]) {
					sprintf_s(sendMsg.message, "Server: User %s connected", msg.message);
					client->setMSG(sendMsg);
					send(client->GetSocket(), client->GetSendBuffer(), PACKETSIZE, NULL);
				}

				sendMsg.type = MT_NEW_ROOM;
				strcpy_s(sendMsg.message, g_RoomName[msg.room_id].c_str());
				pSendClientContext->setMSG(sendMsg);
				send(pSendClientContext->GetSocket(), pSendClientContext->GetSendBuffer(), PACKETSIZE, NULL);
			}
			LeaveCriticalSection(&g_csClientList);
			break;
		case MT_SEND_MESSAGE:
			sendMsg.type = MT_SEND_MESSAGE;
			sendMsg.room_id = msg.room_id;
			sendMsg.status = MS_OK;
			pClientContext->GetClientName(buffer);
			for (const auto client : g_RoomClient[msg.room_id]) {
				if (client == pClientContext)
					continue;
				sprintf_s(sendMsg.message, "%s: %s", buffer, msg.message);
				client->setMSG(sendMsg);
				send(client->GetSocket(), client->GetSendBuffer(), PACKETSIZE, NULL);
			}
			break;

		default:
			//We should never be reaching here, under normal circumstances.
			break;
		} // switch

		pClientContext->ResetWSABUF();

		dwFlags = 0;

		//Get the data.
		nBytesRecv = WSARecv(pClientContext->GetSocket(), p_wbuf, 1,
			&dwBytes, &dwFlags, p_ol, NULL);

		if ((SOCKET_ERROR == nBytesRecv) && (WSA_IO_PENDING != WSAGetLastError()))
		{
			WriteToConsole("\nThread %d: Error occurred while executing WSARecv().", nThreadNo);

			//Let's not work with this client
			RemoveFromClientListAndFreeMemory(pClientContext);
		}

	} // while

	return 0;
}
CClientContext* JoinClient(int room_id, char* clientName) {
	CClientContext* client = NULL;
	auto it = std::find_if(g_ClientContext.begin(), g_ClientContext.end(), [clientName](CClientContext* obj) {char buf[BUFSIZE]; obj->GetClientName(buf);return !strcmp(buf, clientName);});
	if (it != g_ClientContext.end()) {
		g_RoomClient[room_id].push_back(*it);
		client = *it;
	}
	return client;
}
int AddRoom(char* roomName, CClientContext* clientContext) {
	EnterCriticalSection(&g_csClientList);
	int id = 0;
	for (const auto room : g_RoomName) {
		if (room.first > id) {
			break;
		}
		id += 1;
	}
	g_RoomName[id] = std::string(roomName);
	g_RoomClient[id].push_back(clientContext);
	LeaveCriticalSection(&g_csClientList);
	return id;
}

void WriteToConsole(char *szFormat, ...)
{
	EnterCriticalSection(&g_csConsole);

	va_list args;
	va_start(args, szFormat);

	vprintf(szFormat, args);

	va_end(args);

	LeaveCriticalSection(&g_csConsole);
}

//Store client related information in a vector
void AddToClientList(CClientContext  *pClientContext)
{
	EnterCriticalSection(&g_csClientList);
	//Store these structures in vectors
	g_ClientContext.push_back(pClientContext);
	LeaveCriticalSection(&g_csClientList);
}

void RemoveFromClientListAndFreeMemory(CClientContext *pClientContext)
{
	EnterCriticalSection(&g_csClientList);
	ChatMSG sendMsg;
	size_t size;
	char buffer[BUFSIZE];
	g_ClientContext.erase(std::remove(g_ClientContext.begin(), g_ClientContext.end(), pClientContext), g_ClientContext.end());
	for (auto it = g_RoomClient.begin(); it != g_RoomClient.end(); ) {
		auto& v = *it;
		size = v.second.size();
		v.second.erase(std::remove(v.second.begin(), v.second.end(), pClientContext), v.second.end());
		if (v.second.size() == 0) {
			g_RoomName.erase(v.first);
			g_RoomClient.erase(it++);
		}
		else {
			++it;
			if (v.second.size() < size) {
				sendMsg.type = MT_SEND_MESSAGE;
				sendMsg.room_id = v.first;
				sendMsg.status = MS_OK;
				pClientContext->GetClientName(buffer);
				for (const auto client : g_RoomClient[v.first]) {
					sprintf_s(sendMsg.message, "Server: User %s disconnected", buffer);
					client->setMSG(sendMsg);
					send(client->GetSocket(), client->GetSendBuffer(), PACKETSIZE, NULL);
				}
			}
		}
	}
	delete pClientContext;
	LeaveCriticalSection(&g_csClientList);
}

//Clean up the list, this function will be executed at the time of shutdown
void CleanClientList()
{
	EnterCriticalSection(&g_csClientList);
	for (const auto ptr : g_ClientContext) {
		delete ptr;
	}
	g_ClientContext.clear();
	LeaveCriticalSection(&g_csClientList);
}

//The use of static variable will ensure that 
//we will make a call to GetSystemInfo() 
//to find out number of processors, 
//only if we don't have the information already.
//Repeated use of this function will be efficient.
int GetNoOfProcessors()
{
	static int nProcessors = 0;

	if (0 == nProcessors)
	{
		SYSTEM_INFO si;

		GetSystemInfo(&si);

		nProcessors = si.dwNumberOfProcessors;
	}

	return nProcessors;
}