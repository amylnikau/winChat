#include "stdafx.h"
#include "MFCWinChat.h"
#include "MFCWinChatDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BOOL shutdownThread = FALSE;

void HandleError(HWND hWnd, int errcode, TCHAR* where)
{
	TCHAR err[255];
	swprintf_s(err, L"Error has occured in %s.\nError code - %d", where, errcode);
	MessageBox(hWnd, err, L"Error!", MB_ICONERROR | MB_OK);
}

void serialize(ChatMSG& msgPacket, char *data)
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

ChatMSG deserialize(char *data)
{
	ChatMSG msgPacket;
	int *q = (int*)data;
	msgPacket.type = *q;       q++;
	msgPacket.status = *q;       q++;
	msgPacket.room_id = *q;       q++;

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


CNewRoomDlg::CNewRoomDlg() : CDialogEx(IDD_NEWROOM_DIALOG)
{
}
void CNewRoomDlg::SetSelectedUsers(int selItems[], int size) {
	selUsers.clear();
	selUsers.reserve(size);
	for (int i = 0;i < size;++i)
		selUsers.push_back(userList[selItems[i]]);
}
std::vector<CStringA> CNewRoomDlg::GetSelectedUsers() {
	return selUsers;
}
CStringA CNewRoomDlg::GetRoomName() {
	return roomName;
}
void CNewRoomDlg::SetUserList(std::vector<CStringA> newUserList) {
	userList = newUserList;
}
BOOL CNewRoomDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	for (const auto user : userList)
		SendDlgItemMessageA(m_hWnd, IDC_LIST_USER, LB_ADDSTRING, 0, (LPARAM)(LPCSTR)user);
	return TRUE;
}
void CNewRoomDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CNewRoomDlg, CDialogEx)
	ON_BN_CLICKED(IDOK, &CNewRoomDlg::OnBnClickedOk)
END_MESSAGE_MAP()

void CNewRoomDlg::OnBnClickedOk()
{
	int nCount = GetDlgItemTextA(m_hWnd, IDC_ROOMNAME, roomName.GetBuffer(BUFSIZE), BUFSIZE);
	roomName.ReleaseBuffer();
	if (nCount <= 0) {
		AfxMessageBox(_T("Введите название комнаты!"));
		return;
	}
	int selCnt = SendDlgItemMessageA(m_hWnd, IDC_LIST_USER, LB_GETSELCOUNT, 0, 0);
	if (selCnt <= 0) {
		AfxMessageBox(_T("Вы не выбрали пользователей!"));
		return;
	}
	int* selItems = new int[selCnt];
	SendDlgItemMessageA(m_hWnd, IDC_LIST_USER, LB_GETSELITEMS, (WPARAM)selCnt, (LPARAM)selItems);
	SetSelectedUsers(selItems, selCnt);
	delete[] selItems;
	CDialogEx::OnOK();
}

CWinChatDlg::CWinChatDlg(CWnd* pParent)
	: CDialogEx(IDD_MFCWINCHAT_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CWinChatDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_ROOM, m_ListboxRoom);
}

BEGIN_MESSAGE_MAP(CWinChatDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(ID_SEND_BUTTON, &CWinChatDlg::OnBnClickedSendButton)
	ON_BN_CLICKED(IDC_NEW_ROOM, &CWinChatDlg::OnBnClickedNewRoom)
	ON_MESSAGE(WM_LIST_USER, &CWinChatDlg::OnListUser)
	ON_MESSAGE(WM_SEND_USER_LIST, &CWinChatDlg::OnSendUserList)
	ON_LBN_SELCHANGE(IDC_LIST_ROOM, &CWinChatDlg::OnLbnSelchangeListRoom)
	ON_COMMAND(ID_EXIT, &CWinChatDlg::OnExit)
END_MESSAGE_MAP()

BOOL CWinChatDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	if (InitWSA())
	{

		unsigned thridE;
		socketThread = (HANDLE)_beginthreadex(NULL,//must be FOR W95 ,SA
			0,//stack size committed
			SocketProc,
			(void *)this, //*arglist
			0,//0
			&thridE
		);
		return TRUE;
	}
	else {
		EndDialog(-1);
		return FALSE;
	}


}

CWinChatDlg::~CWinChatDlg() {
	shutdownThread = true;
	closesocket(client_socket);
	WaitForSingleObject(socketThread, INFINITE);
	CloseHandle(socketThread);
	WSACleanup();
}

void CWinChatDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this);

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

HCURSOR CWinChatDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

BOOL CWinChatDlg::InitWSA()
{
	WSADATA wsadata;
	char* address = "127.0.0.1";
	char clientName[BUFSIZE];
	sprintf_s(clientName, "Anonymous%d", GetCurrentProcessId());
	SetDlgItemTextA(m_hWnd, IDC_STATICNAME, clientName);
	int retcode = WSAStartup(WINSOCK_VERSION, &wsadata);
	if (retcode != 0) {
		HandleError(m_hWnd, retcode, L"WSAStartup");
		return false;
	}
	client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (client_socket == INVALID_SOCKET) {
		HandleError(m_hWnd, WSAGetLastError(), L"socket");
		return false;
	}

	sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(DEFAULT_PORT);
	InetPtonA(AF_INET, address, &(sa.sin_addr));

	retcode = connect(client_socket, (SOCKADDR *)& sa, sizeof(sa));
	if (retcode == SOCKET_ERROR) {
		closesocket(client_socket);
		HandleError(m_hWnd, retcode, L"connect");
		WSACleanup();
		return false;
	}

	if (client_socket == INVALID_SOCKET) {
		HandleError(m_hWnd, retcode, L"connect");
		WSACleanup();
		return false;
	}
	ChatMSG msg;
	char message[PACKETSIZE];
	msg.type = MT_LOGIN;
	strcpy_s(msg.message, clientName);
	serialize(msg, message);
	send(client_socket, message, PACKETSIZE, 0);

	return true;
}


void CWinChatDlg::OnBnClickedSendButton()
{

	if (currentRoom != -1) {
		char buffer[BUFSIZE];
		char message[PACKETSIZE];
		char editMessage[BUFSIZE + 10];
		ChatMSG msg;
		time_t rawtime;
		struct tm timeinfo;
		time(&rawtime);
		localtime_s(&timeinfo, &rawtime);
		int nRecv = GetDlgItemTextA(m_hWnd, IDC_EDIT_MESSAGE, buffer, BUFSIZE);
		if (nRecv <= 0) {
			return;
		}
		msg.type = MT_SEND_MESSAGE;
		msg.room_id = currentRoom;
		strcpy_s(msg.message, buffer);
		serialize(msg, message);
		int iResult = send(client_socket, message, PACKETSIZE, 0);

		if (iResult <= 0)
			HandleError(m_hWnd, WSAGetLastError(), L"send()");

		sprintf_s(editMessage, "(%.2d:%.2d) ", timeinfo.tm_hour, timeinfo.tm_min);
		strcat_s(editMessage, "You: ");
		strcat_s(editMessage, msg.message);
		strcat_s(editMessage, "\r\n");
		chatMessages[currentRoom] += editMessage;
		int nLength = GetWindowTextLengthA(::GetDlgItem(m_hWnd, IDC_CHAT_MESSAGE));
		SetDlgItemTextA(m_hWnd, IDC_EDIT_MESSAGE, "");
		SendDlgItemMessageA(m_hWnd, IDC_CHAT_MESSAGE, EM_SETSEL, nLength, nLength);
		SendDlgItemMessageA(m_hWnd, IDC_CHAT_MESSAGE, EM_REPLACESEL, 0, (LPARAM)editMessage);
		SendMessageA(m_hWnd, WM_NEXTDLGCTL, (WPARAM)::GetDlgItem(m_hWnd, IDC_EDIT_MESSAGE), TRUE);
	}
	else {
		AfxMessageBox(_T("Вы не выбрали комнату!"));
	}

}

unsigned int __stdcall  SocketProc(void *param)
{
	CWinChatDlg& chatDlg = *(CWinChatDlg*)param;
	HWND hWnd = chatDlg.m_hWnd;
	SOCKET client_socket = chatDlg.client_socket;
	std::map<CStringA, int>& rooms = chatDlg.rooms;
	std::map<int, CStringA>& chatMessages = chatDlg.chatMessages;
	int& currentRoom = chatDlg.currentRoom;
	std::vector<CStringA> listUser;
	while (!shutdownThread) {
		char buffer[PACKETSIZE];
		char editMessage[BUFSIZE + 10];
		time_t rawtime;
		struct tm timeinfo;
		int nLength;
		int nRecvBytes = recv(client_socket, buffer, PACKETSIZE, NULL);
		if (nRecvBytes < PACKETSIZE)
			return 0;
		time(&rawtime);
		localtime_s(&timeinfo, &rawtime);

		ChatMSG msg = deserialize(buffer);
		switch (msg.type) {
		case MT_LIST_USER:
			if (msg.status != MS_OK) {
				if (msg.status == MS_LAST)
					listUser.push_back(msg.message);
				SendMessageA(hWnd, WM_LIST_USER, 0, (LPARAM)&listUser);
				listUser.clear();
			}
			else {
				listUser.push_back(msg.message);
			}
			break;
		case MT_NEW_ROOM:
			if (msg.status != MS_NODATA)
			{
				rooms[msg.message] = msg.room_id;
				SendDlgItemMessageA(hWnd, IDC_LIST_ROOM, LB_ADDSTRING, 0, (LPARAM)msg.message);
			}
			else
				SendMessageA(hWnd, WM_SEND_USER_LIST, 0, (LPARAM)msg.room_id);
			break;
		case MT_SEND_MESSAGE:
			sprintf_s(editMessage, "(%.2d:%.2d) ", timeinfo.tm_hour, timeinfo.tm_min);
			strcat_s(editMessage, msg.message);
			strcat_s(editMessage, "\r\n");
			chatMessages[msg.room_id] += editMessage;
			if (currentRoom == msg.room_id) {
				nLength = GetWindowTextLengthA(GetDlgItem(hWnd, IDC_CHAT_MESSAGE));
				SendDlgItemMessageA(hWnd, IDC_CHAT_MESSAGE, EM_SETSEL, nLength, nLength);
				SendDlgItemMessageA(hWnd, IDC_CHAT_MESSAGE, EM_REPLACESEL, 0, (LPARAM)editMessage);
			}
			break;
		}
	}
	return 0;
}

void CWinChatDlg::OnBnClickedNewRoom()
{
	ChatMSG msg;
	char message[PACKETSIZE];
	msg.type = MT_LIST_USER;
	serialize(msg, message);
	send(client_socket, message, PACKETSIZE, 0);
}

void CWinChatDlg::OnLbnSelchangeListRoom()
{
	CStringA theData;
	// get the selected position of the listbox
	UINT uiSelection = m_ListboxRoom.GetCurSel();

	if (uiSelection == LB_ERR) return;

	SendDlgItemMessageA(m_hWnd, IDC_LIST_ROOM, LB_GETTEXT, uiSelection, (LPARAM)theData.GetBuffer(BUFSIZE));
	theData.ReleaseBuffer();
	int room_id = rooms[theData];
	currentRoom = room_id;
	SetDlgItemTextA(m_hWnd, IDC_CHAT_MESSAGE, chatMessages[room_id]);
}

LRESULT CWinChatDlg::OnListUser(WPARAM wParam, LPARAM lParam)
{
	std::vector<CStringA> userList = *(std::vector<CStringA>*)lParam;

	newRoomDlg.SetUserList(userList);
	INT_PTR nResponse = newRoomDlg.DoModal();
	if (nResponse == IDOK)
	{
		ChatMSG msg;
		char message[PACKETSIZE];
		msg.type = MT_NEW_ROOM;
		strcpy_s(msg.message, newRoomDlg.GetRoomName());
		serialize(msg, message);
		send(client_socket, message, PACKETSIZE, 0);

	}
	return TRUE;
}

LRESULT CWinChatDlg::OnSendUserList(WPARAM wParam, LPARAM lParam)
{
	int room_id = (int)lParam;
	CStringA roomName = newRoomDlg.GetRoomName();
	rooms[roomName] = room_id;
	ChatMSG msg;
	char message[PACKETSIZE];
	msg.type = MT_JOIN_USER;
	msg.room_id = room_id;
	for (const auto user : newRoomDlg.GetSelectedUsers()) {
		strcpy_s(msg.message, user);
		serialize(msg, message);
		send(client_socket, message, PACKETSIZE, 0);
	}
	SendDlgItemMessageA(m_hWnd, IDC_LIST_ROOM, LB_ADDSTRING, 0, (LPARAM)(LPCSTR)roomName);
	SendDlgItemMessageA(m_hWnd, IDC_LIST_ROOM, LB_SETCURSEL, rooms.size() - 1, 0);
	OnLbnSelchangeListRoom();
	return TRUE;
}

void CWinChatDlg::OnExit()
{
	EndDialog(0);
}
