#include <afxwin.h>   //MFC core and standard components
#include "resource.h"  //main symbols
#include <pdh.h>
#pragma comment(lib, "pdh.lib")

//Globals
#define UPDATE_INTERVAL_HDD 33
#define READ_TRIGGER_BYTES 1024
#define WRITE_TRIGGER_BYTES 1024
#define READWRITE_TRIGGER_BYTES READ_TRIGGER_BYTES+WRITE_TRIGGER_BYTES
#define COUNTER_TOTAL_READ "\\PhysicalDisk(_Total)\\Disk Read Bytes/sec"
#define COUNTER_TOTAL_WRITE "\\PhysicalDisk(_Total)\\Disk Write Bytes/sec"
NOTIFYICONDATA	niData;
HICON hIconR, hIconW, hIconRW, hIconIdle, hIconDisabled;
enum HDDState { HDD_DISABLED = 0, HDD_IDLE, HDD_READ, HDD_WRITE, HDD_READWRITE };

//HDD Stat polling/updating routines
void UpdateHDDIcon(HDDState state){
	switch (state)
	{
	case HDD_IDLE:
		niData.hIcon = hIconIdle;
		break;
	case HDD_READ:
		niData.hIcon = hIconR;
		break;
	case HDD_WRITE:
		niData.hIcon = hIconW;
		break;
	case HDD_READWRITE:
		niData.hIcon = hIconRW;
		break;
	default:
		niData.hIcon = hIconDisabled;
		break;
	}
	Shell_NotifyIcon(NIM_MODIFY, &niData);
};

void UpdateHDDStat(double bRead, double bWrite) {
	if (bRead < 0 || bWrite < 0){
		UpdateHDDIcon(HDD_DISABLED);
		return;
	}
	if ((bRead + bWrite) >= READWRITE_TRIGGER_BYTES){
		if (bRead / bWrite > 2){
			UpdateHDDIcon(HDD_READ);
			return;
		}
		if (bWrite / bRead > 2){
			UpdateHDDIcon(HDD_WRITE);
			return;
		}
		UpdateHDDIcon(HDD_READWRITE);
	}
	else if (bRead > READ_TRIGGER_BYTES)
		UpdateHDDIcon(HDD_READ);
	else if (bWrite > WRITE_TRIGGER_BYTES)
		UpdateHDDIcon(HDD_WRITE);
	else
		UpdateHDDIcon(HDD_IDLE);
}

int GetHDDInfoInit(){
	HQUERY hQuery = NULL;
	PDH_STATUS pdhStatus;
	HCOUNTER hCounterRead, hCounterWrite;
	// Open a query object.
	pdhStatus = PdhOpenQuery(NULL, 0, &hQuery);
	if (pdhStatus != ERROR_SUCCESS)
		return MessageBox(NULL, "PdhOpenQuery failed.", "Error", MB_OK | MB_ICONEXCLAMATION);
	// Get HDD activity data. My machine's second drive bay is empty.
	PDH_STATUS pdhStatusR = PdhAddCounter(hQuery, COUNTER_TOTAL_READ, 0, &hCounterRead);
	PDH_STATUS pdhStatusW = PdhAddCounter(hQuery, COUNTER_TOTAL_WRITE, 0, &hCounterWrite);
	if (pdhStatusR != ERROR_SUCCESS || pdhStatusR != ERROR_SUCCESS)
		return MessageBox(NULL, "PdhAddCounter failed.", "Error", MB_OK | MB_ICONEXCLAMATION);
	pdhStatus = PdhCollectQueryData(hQuery);
	// Infinite loop for HDD activity data query
	for (;;){
		ULONG CounterType;
		PDH_FMT_COUNTERVALUE DisplayValue;

		double bRead, bWrite;
		pdhStatus = PdhCollectQueryData(hQuery);
		pdhStatus = PdhGetFormattedCounterValue(hCounterRead, PDH_FMT_DOUBLE, &CounterType, &DisplayValue);
		bRead = (pdhStatus == ERROR_SUCCESS) ? DisplayValue.doubleValue : -1;
		pdhStatus = PdhGetFormattedCounterValue(hCounterWrite, PDH_FMT_DOUBLE, &CounterType, &DisplayValue);
		bWrite = (pdhStatus == ERROR_SUCCESS) ? DisplayValue.doubleValue : -1;
		UpdateHDDStat(bRead, bWrite);
		Sleep(UPDATE_INTERVAL_HDD);
	}
}

//MFC UI dialogs
//Message handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIconRW);
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return DefWindowProc(hDlg, message, wParam, lParam);
}

class HDDActivity : public CDialog
{
	BOOL m_visible;
	HWND hwnd_About = NULL;

public:
	HDDActivity(CWnd* pParent = NULL) : CDialog(dlg_hidden_main, pParent) { };

protected:
	BOOL OnInitDialog()
	{
		CDialog::OnInitDialog();
		m_visible = false;

		//Initial HDD state is disabled
		hIconR = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(icon_read));
		hIconW = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(icon_write));
		hIconRW = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(icon_readwrite));
		hIconIdle = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(icon_idle));
		hIconDisabled = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(icon_disabled));
		niData.hIcon = hIconDisabled;
		niData.hWnd = m_hWnd;
		niData.uCallbackMessage = WM_APP;
		niData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
		strncpy(niData.szTip, "Put indicator LEDs back on Laptops!", sizeof(niData.szTip) / sizeof(TCHAR));
		Shell_NotifyIcon(NIM_ADD, &niData);
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)GetHDDInfoInit, 0, 0, 0);
		return true;
	}

	void ShowAboutDlg(){
		if (hwnd_About){
			::DestroyWindow(hwnd_About);
		}
		hwnd_About = CreateDialog(AfxGetInstanceHandle(), MAKEINTRESOURCE(dlg_about), m_hWnd, (DLGPROC)About);
	}

	void ShowContextMenu(){
		POINT pt;
		GetCursorPos(&pt);
		CMenu menu;
		VERIFY(menu.LoadMenu(IDR_MENU1));
		CMenu* pPopup = menu.GetSubMenu(0);
		SetForegroundWindow();
		pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, AfxGetMainWnd());
	}

	void OnWindowPosChanging(WINDOWPOS FAR* lpwndpos)
	{
		if (!m_visible){
			lpwndpos->flags &= ~SWP_SHOWWINDOW;
		}
		CDialog::OnWindowPosChanging(lpwndpos);
	}

	LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (message == WM_APP){
			switch (lParam){
			case WM_LBUTTONDBLCLK:
				ShowAboutDlg();
				return TRUE;
			case WM_RBUTTONDOWN:
			case WM_CONTEXTMENU:
				ShowContextMenu();
				break;
			}
		}
		return CDialog::WindowProc(message, wParam, lParam);
	}

public:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnContextMenuAbout();
	afx_msg void OnContextMenuExit();
};

BEGIN_MESSAGE_MAP(HDDActivity, CDialog)
	ON_WM_WINDOWPOSCHANGING()
	ON_COMMAND(ID_CXTMENU_ABOUT, &HDDActivity::OnContextMenuAbout)
	ON_COMMAND(ID_CXTMENU_EXIT, &HDDActivity::OnContextMenuExit)
END_MESSAGE_MAP()

class MainApp : public CWinApp
{
public:
	MainApp() {};
	virtual BOOL InitInstance()
	{
		CWinApp::InitInstance();
		HDDActivity dlg;
		m_pMainWnd = &dlg;
		INT_PTR nResponse = dlg.DoModal();
		return false;
	}
};

//Start application
MainApp theApp;


void HDDActivity::OnContextMenuAbout()
{
	ShowAboutDlg();
}


void HDDActivity::OnContextMenuExit()
{
	Shell_NotifyIcon(NIM_DELETE, &niData);
	PostQuitMessage(0);
}
