#pragma once
#include "afxwin.h"
#define BUFSIZE 512 
#define PACKETSIZE sizeof(ChatMSG)
#define DEFAULT_PORT 3504
#define WM_LIST_USER (WM_APP + 1)
#define WM_SEND_USER_LIST (WM_APP + 2)

class CNewRoomDlg : public CDialogEx
{
public:
	CNewRoomDlg();
	void SetUserList(std::vector<CStringA> newUserList);
	void SetSelectedUsers(int selItems[], int size);
	std::vector<CStringA> GetSelectedUsers();
	CStringA GetRoomName();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_NEWROOM_DIALOG };
#endif

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	DECLARE_MESSAGE_MAP()
private:
	CStringA roomName;
	std::vector<CStringA> userList;
	std::vector<CStringA> selUsers;
public:
	afx_msg void OnBnClickedOk();
};

class CWinChatDlg : public CDialogEx
{
public:
	CWinChatDlg(CWnd* pParent = NULL);
	~CWinChatDlg();
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MFCWINCHAT_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	HICON m_hIcon;

	virtual BOOL OnInitDialog();
	afx_msg LRESULT OnListUser(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnSendUserList(WPARAM wParam, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	std::map<CStringA, int> rooms;
	std::map<int, CStringA> chatMessages;
	int currentRoom = -1;
	SOCKET client_socket;
	afx_msg void OnBnClickedSendButton();
	afx_msg void OnBnClickedNewRoom();
	afx_msg void OnLbnSelchangeListRoom();

private:
	CNewRoomDlg newRoomDlg;
	HANDLE socketThread;
	BOOL InitWSA();
	CListBox m_ListboxRoom;
public:
	afx_msg void OnExit();
};

enum MESSAGE_TYPES
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

void HandleError(HWND hWnd, int errcode, TCHAR* where);
unsigned int __stdcall  SocketProc(void *param);
void serialize(ChatMSG& msgPacket, char *data);
ChatMSG deserialize(char *data);