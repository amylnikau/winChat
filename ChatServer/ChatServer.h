#pragma once

#define WORKER_THREADS_PER_PROCESSOR 2

//Buffer Length 
#define BUFSIZE 512
#define PACKETSIZE sizeof(ChatMSG)
#define DEFAULT_PORT 3504

//Graceful shutdown Event
//For this simple implementation,
//We can use global variable as well.
//Wanted to demonstrate use of event
//for shutdown
HANDLE g_hShutdownEvent = NULL;

//Number of threads to be created.
int g_nThreads = 0;

//To store handle of worker threads
HANDLE *g_phWorkerThreads = NULL;

//Handle for Accept related thread
HANDLE g_hAcceptThread = NULL;

//Network Event for Accept
WSAEVENT g_hAcceptEvent;

CRITICAL_SECTION g_csConsole; //When threads write to console we need mutual exclusion
CRITICAL_SECTION g_csClientList; //Need to protect the client list

//Global I/O completion port handle
HANDLE g_hIOCompletionPort = NULL;

enum MESSAGE_TYPE
{
	MT_LOGIN,
	MT_SEND_MESSAGE,
	MT_LIST_USER,
	MT_NEW_ROOM,
	MT_JOIN_USER,
	MT_DISCONNECT_USER
};

enum MESSAGE_STATUS
{
	MS_OK,
	MS_LAST,
	MS_NODATA
};

struct ChatMSG
{
	int type;
	int status;
	int room_id;
	char message[BUFSIZE];
};


class CClientContext  //To store and manage client related information
{
private:

	OVERLAPPED        *m_pol;
	WSABUF            *m_pwbuf;

	char			  m_ClientName[BUFSIZE];

	SOCKET            m_Socket;  //accepted socket
	char              m_szBuffer[PACKETSIZE];
	char              m_szSendBuffer[PACKETSIZE];

	static void serialize(ChatMSG& msgPacket, char *data)
	{
		int *q = (int*)data;
		*q = msgPacket.type;       q++;
		*q = msgPacket.status;       q++;
		*q = msgPacket.room_id;       q++;

		char *p = (char*)q;
		int i = 0;
		while (i < BUFSIZE)
		{
			*p = msgPacket.message[i];
			p++;
			i++;
		}
	}

	static ChatMSG deserialize(char *data)
	{
		ChatMSG msgPacket;
		int *q = (int*)data;
		msgPacket.type = *q;       q++;
		msgPacket.status = *q;    q++;
		msgPacket.room_id = *q;    q++;

		char *p = (char*)q;
		int i = 0;
		while (i < BUFSIZE)
		{
			msgPacket.message[i] = *p;
			p++;
			i++;
		}
		return msgPacket;
	}

public:

	void SetClientName(char* clientName)
	{
		strcpy_s(m_ClientName, clientName);
	}

	void GetClientName(char* clientName)
	{
		strcpy_s(clientName, BUFSIZE, m_ClientName);
	}

	void SetSocket(SOCKET s)
	{
		m_Socket = s;
	}

	SOCKET GetSocket()
	{
		return m_Socket;
	}

	char* GetSendBuffer()
	{
		return m_szSendBuffer;
	}

	void ZeroBuffer()
	{
		ZeroMemory(m_szBuffer, PACKETSIZE);
	}

	WSABUF* GetWSABUFPtr()
	{
		return m_pwbuf;
	}

	OVERLAPPED* GetOVERLAPPEDPtr()
	{
		return m_pol;
	}

	void ResetWSABUF()
	{
		ZeroBuffer();
		m_pwbuf->buf = m_szBuffer;
		m_pwbuf->len = PACKETSIZE;
	}

	ChatMSG GetMSG() {
		return deserialize(m_szBuffer);
	}

	void setMSG(ChatMSG msg) {
		ZeroMemory(m_szSendBuffer, PACKETSIZE);
		serialize(msg, m_szSendBuffer);
	}

	//Constructor
	CClientContext()
	{
		m_pol = new OVERLAPPED;
		m_pwbuf = new WSABUF;

		ZeroMemory(m_pol, sizeof(OVERLAPPED));

		m_Socket = INVALID_SOCKET;

		ZeroMemory(m_szBuffer, PACKETSIZE);
		ZeroMemory(m_szSendBuffer, PACKETSIZE);
		ZeroMemory(m_ClientName, BUFSIZE);

		m_pwbuf->buf = m_szBuffer;
		m_pwbuf->len = PACKETSIZE;

	}

	//destructor
	~CClientContext()
	{
		shutdown(m_Socket, SD_BOTH);
		closesocket(m_Socket);

		//Cleanup
		delete m_pol;
		delete m_pwbuf;
	}
};

//Vector to store pointers of dynamically allocated ClientContext.
//map class can also be used.
//Link list can also be created.
std::vector<CClientContext *> g_ClientContext;
std::map<int, std::vector<CClientContext *>> g_RoomClient;
std::map<int, std::string> g_RoomName;

//global functions
bool InitializeIOCP();
bool Initialize();
void CleanUp();
void DeInitialize();
DWORD WINAPI AcceptThread(LPVOID lParam);
void AcceptConnection(SOCKET ListenSocket);
bool AssociateWithIOCP(CClientContext   *pClientContext);
DWORD WINAPI WorkerThread(LPVOID lpParam);
void WriteToConsole(char *szFormat, ...);
CClientContext* JoinClient(int room_id, char* clientName);
int AddRoom(char* roomName, CClientContext* clientContext);
void AddToClientList(CClientContext  *pClientContext);
void RemoveFromClientListAndFreeMemory(CClientContext *pClientContext);
void CleanClientList();
int GetNoOfProcessors();
