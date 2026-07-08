// SecurityRun.cpp : Defines the entry point for the application.
//

#include "stdafx.h"

#include <atlstr.h>
#include <atlbase.h>
#include <atlwin.h>
#include <atlcoll.h>
#include <atlcom.h>

#include "exdisp.h"
#include "exdispid.h"
#include "mshtml.h"

#include <strsafe.h>
#include <list>
#include <conio.h>
#include <assert.h>
#include <Winver.h>
#include <commctrl.h>
#include <shellapi.h>
#include "SecurityRun.h"
#include "HookLib\Hook.h"
#include "RestrRun\RestrictingToken.h"
#include "ADD\DefaultBrowser.h"
#include "HookLib\MappingFile.h"
#include "MFile.h"
#include "DefineAdd.h"
#include "WTL\atlapp.h"
#include "WTL\atlctrls.h"
#include "WTL\atlctrlx.h"
#include "ADD\IconTools.h"

#include "ADD\Log.h"

#define MAX_LOADSTRING			100

// REESTR
#define STR_KEY								_T("Key")
#define STR_DLGRUNSHOW						_T("RunShow")
#define STR_DLGWHILESHOW					_T("WhileShow")
#define STR_DLGISWHILE						_T("While")
#define STR_CHECKISDEF						_T("CheckAsDef")
#define STR_LOG								_T("Log")
#define STR_OPTION							_T("-OPTION ")
#define STR_NOSPLASH						_T("-NOSPLASH ")
#define STR_NOCAPTION_ADD					_T("-NOCAPTION ")
#define STR_INSTALL							_T("-INSTALL ")
#define STR_UNINSTALL						_T("-UNINSTALL ")
#define STR_EMBEDDING						_T("-EMBEDDING ")
#define STR_POINT							_T(".")
#define STR_COMMA							_T(",")
#define STR_SPACE							_T(" ")
#define STR_SAVE_SD_N						_T("SaveSD%d")
#define STR_SAVE_SD_SIZE_N					_T("SaveSDSize%d")
#define STR_HOOK_CBT_NAME					_T("GetHookMessageCBT")
#define STR_HOOK_PROC_RET					_T("GetProcRet")
#define STR_IEXPLORE_EXE_1					_T("IEXPLORE.EXE")
#define STR_IEXPLORE_EXE_2					_T("IEXPLORE.EXE\"")

#define MAX_VALUE_NAME						17000
#define MIN_WHILE_VALUE						3
#define MAX_NUM_DIGITS						2

// Global Variables:
HINSTANCE hInst = NULL;								// current instance
TCHAR szTitle[MAX_LOADSTRING] = _T("");				// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING] = _T("");		// the main window class name
BOOL g_bNOCaption_ADD = TRUE;

struct HOOK_ID
{
	HWND	hWnd;
	HHOOK	hHook;
	HANDLE	hProcess;
	HANDLE  hThread;
	HOOK_ID(){hWnd = NULL; hHook = NULL;hProcess = NULL;hThread = NULL;}
};

struct PROGRAMM_INF
{
	TCHAR programRun[_MAX_DRIVE + _MAX_PATH + _MAX_FNAME + _MAX_EXT];
	UINT timerId;
};

enum __ADD
{
	IS_WHILE = 0,
	IS_NOT_WHILE,
};

const TCHAR *g_ListClassesNotHOOK[] = {
							_T("IEHelper Object"),
							_T("EWHAcrobatSessionWindow"),
							_T("EWHAcrobatSenderWindow"),
							_T("NewBrowserSink"),
							_T("atlcommon_timer"),
							_T("VideoRenderer"),
							_T("FilterGraphWindow"),							
};

///////
//
const TCHAR *g_ListPSDModify[] = {
//							_T("software\\classes\\clsid\\{BDAD1DAD-C946-4A17-ADC1-64B5B4FF55D0}\\InprocServer32"),
//							_T("software\\microsoft\\Internet Explorer\\ToolBar"),
							_T("software\\classes\\clsid\\{73B24247-042E-4EF5-ADC2-42F62E6FD654}\\InprocServer32"),
};

const HKEY g_ListHKEYForPSD[] = {
//							HKEY_LOCAL_MACHINE,
//							HKEY_LOCAL_MACHINE,
							HKEY_LOCAL_MACHINE,
};

/*
const TCHAR *g_ListDellToolBar[] = {
							_T("software\\microsoft\\Internet Explorer\\ToolBar\\WebBrowser\\{BDAD1DAD-C946-4A17-ADC1-64B5B4FF55D0}"),
							_T(""),
};
*/

#define COUNT_HKEY			(sizeof(g_ListHKEYForPSD) / sizeof(HKEY))
#define COUNT_SAVE_PSD		(sizeof(g_ListPSDModify) / sizeof(TCHAR*))
PSECURITY_DESCRIPTOR pSDRestore[COUNT_SAVE_PSD] = {0};
//
///////

CAtlArray <HICON> g_listBigIcon;
CAtlArray <HICON> g_listSmallIcon;
CAtlArray <HOOK_ID> g_listInfo;
CAtlArray <PROGRAMM_INF> g_listFileNotRun;

CMFile g_MFile;
CHook g_Hook;
HWND g_FindHWnd = NULL;
HWND g_hWnd = NULL;
HHOOK gHookCBT = NULL;
UINT TimerID = 1;
BOOL g_bIsSecuriRun = FALSE;
TCHAR APLICATION[MAX_LOADSTRING] = _T("");
BOOL g_bEmbedding = FALSE;

HICON g_BigIcon = NULL;
HICON g_SmallIcon = NULL;

CAtlString g_strLaunch = _T("");

CLog g_log;
CLog g_logHook;
int g_NumHookHWND = 0;

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int, LPTSTR);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	OptionProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	DlgRunProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	DlgRunProcNew(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	DlgSetAsDefaultProc(HWND, UINT, WPARAM, LPARAM);

void DeleteSaveSD(int num)
{
	CAtlString strSubKey = _T("");
	CAtlString strParamName = _T("");
	strSubKey.Format(STR_PROGRAMM_SAVE_FOLDER_MAIN, APLICATION);
	CRegKey Key;
	strParamName.Format(STR_SAVE_SD_N, num);

	SHDeleteValue(HKEY_CURRENT_USER, strSubKey, strParamName);
	
}
BOOL IsSavedSD(int num)
{
	CAtlString strSubKey = _T("");
	CAtlString strParamName = _T("");
	strSubKey.Format(STR_PROGRAMM_SAVE_FOLDER_MAIN, APLICATION);
	CRegKey Key;

	strParamName.Format(STR_SAVE_SD_N, num);
	if (Key.Open(HKEY_CURRENT_USER, strSubKey, KEY_ALL_ACCESS) == ERROR_SUCCESS)
	{
		DWORD size = 0;
		if (Key.QueryValue(strParamName, NULL, NULL, &size) == ERROR_SUCCESS)
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL SaveSDToReg(int num, PSECURITY_DESCRIPTOR pSD)
{
	DWORD dwSizeSD = 0;
	HKEY hKey = NULL;
	CRegKey Key;
	LPTSTR StringSecurityDescriptor = {0};

	if (ConvertSecurityDescriptorToStringSecurityDescriptor(pSD, SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &StringSecurityDescriptor, NULL))
	{
		CAtlString strSubKey = _T("");
		CAtlString strParamName = _T("");
		strSubKey.Format(STR_PROGRAMM_SAVE_FOLDER_MAIN, APLICATION);
		CRegKey Key;

		strParamName.Format(STR_SAVE_SD_N, num);
		if (Key.Open(HKEY_CURRENT_USER, strSubKey, KEY_ALL_ACCESS) == ERROR_SUCCESS)
		{
			DWORD size = 0;
			if (Key.SetStringValue(strParamName, StringSecurityDescriptor) == ERROR_SUCCESS)
			{				
				return TRUE;
			}
		}
		LocalFree(StringSecurityDescriptor);
	}
	DWORD error = GetLastError();
	return FALSE;
}

BOOL GetSD(int num, PSECURITY_DESCRIPTOR &pSD)
{
	CAtlString strSubKey = _T("");
	CAtlString strParamName = _T("");
	strSubKey.Format(STR_PROGRAMM_SAVE_FOLDER_MAIN, APLICATION);
	CRegKey Key;
	TCHAR *buffer = NULL;

	strParamName.Format(STR_SAVE_SD_N, num);
	if (Key.Open(HKEY_CURRENT_USER, strSubKey, KEY_ALL_ACCESS) == ERROR_SUCCESS)
	{
		DWORD size = 0;
		if (Key.QueryValue(strParamName, NULL, buffer, &size) == ERROR_SUCCESS)
		{
			buffer = new TCHAR[size];
			if (Key.QueryStringValue(strParamName, buffer, &size) == ERROR_SUCCESS)
			{
				if (ConvertStringSecurityDescriptorToSecurityDescriptor(buffer, SDDL_REVISION_1, &pSD, NULL))
				{
					delete[] buffer;
					return TRUE;
				}
			}			
		}
	}
	return FALSE;
}

void DeletePermission()
{
	DWORD dwSizeSD = 0;
	HKEY hKey = NULL;
	CRegKey Key;

	assert(COUNT_SAVE_PSD > 0);
	assert(COUNT_SAVE_PSD == COUNT_HKEY);

	for (int i = 0; i < COUNT_SAVE_PSD; i++)
	{
		if (Key.Open(g_ListHKEYForPSD[i], g_ListPSDModify[i], KEY_ALL_ACCESS) == ERROR_SUCCESS)
		{
			LONG lRez = Key.GetKeySecurity(DACL_SECURITY_INFORMATION, pSDRestore[i], &dwSizeSD);
			if ((lRez == ERROR_INSUFFICIENT_BUFFER) || (lRez == ERROR_SUCCESS))
			{
				if (!IsSavedSD(i))
				{					
//					pSDRestore[i] = new BYTE[dwSizeSD];
					pSDRestore[i] = LocalAlloc(LPTR, dwSizeSD);
					if (Key.GetKeySecurity(DACL_SECURITY_INFORMATION, pSDRestore[i], &dwSizeSD) == ERROR_SUCCESS)
					{
						CDacl dacl;
						CSecurityDesc sd;
						sd = *(SECURITY_DESCRIPTOR*)pSDRestore[i];
						dacl.AddAllowedAce(Sids::Admins(), GENERIC_ALL, CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE);
						sd.SetDacl(dacl);
						if (Key.SetKeySecurity(DACL_SECURITY_INFORMATION, (PSECURITY_DESCRIPTOR)sd.GetPSECURITY_DESCRIPTOR()) != ERROR_SUCCESS)
						{
							LocalFree(pSDRestore[i]);
//							delete[] pSDRestore[i];
							pSDRestore[i] = NULL;
						}
						SaveSDToReg(i, pSDRestore[i]);
					}
					else
					{
						LocalFree(pSDRestore[i]);
//						delete[] pSDRestore[i];
						pSDRestore[i] = NULL;
					}
				}
				else
				{
					if (!GetSD(i, pSDRestore[i]))
					{
					}
				}
			}
			Key.Close();
		}
	}

//	SHDeleteValue
}

void RestorePermission()
{
	HKEY hKey = NULL;
	CRegKey Key;

	assert(COUNT_SAVE_PSD > 0);
	assert(COUNT_SAVE_PSD == COUNT_HKEY);

	for (int i = 0; i < COUNT_SAVE_PSD; i++)
	{
		if (Key.Open(g_ListHKEYForPSD[i], g_ListPSDModify[i], KEY_ALL_ACCESS) == ERROR_SUCCESS)
		{
			if (pSDRestore[i] != NULL)
			{
				if (Key.SetKeySecurity(DACL_SECURITY_INFORMATION, pSDRestore[i]) != ERROR_SUCCESS)
				{
					assert(!_T("Error Restore Permission -> SetKeySecurity"));
				}
				LocalFree(pSDRestore[i]);
//				delete[] pSDRestore[i];
				pSDRestore[i] = NULL;
				DeleteSaveSD(i);
			}
			Key.Close();
		}
	}
}

void ModifyIcon(HWND hWnd, int numHWnd)
{
	CWindow wnd;	
	BOOL bExtract = FALSE;
	if (IsWindow(hWnd))// && IsWindowVisible(hWnd))
	{
		wnd.Attach(hWnd);
		g_logHook.Save(_T("MODIFY ICON BEGIN\t\tHWND(0x%.08X) \t\tNum(%d)"), (unsigned int)hWnd, numHWnd);

		HICON hIconBig = NULL;
		HICON hIconSmall = NULL;
		hIconSmall = icon_tools::GetWndIcon(hWnd, 0);
		hIconBig = icon_tools::GetWndIcon(hWnd, 1);

		if ((!hIconBig) && (!hIconSmall))
		{
			icon_tools::ExtractIconByHWND(hWnd, &hIconBig, &hIconSmall);
			bExtract = TRUE;
		}
		
//		if ((!hIconBig) && (hIconSmall))
//			hIconBig = hIconSmall;
//			iconTools.ExtractIconByHWND(hWnd, &hIconBig, NULL);
//		if ((!hIconSmall) && (hIconBig))
//			hIconSmall = hIconBig;
//			iconTools.ExtractIconByHWND(hWnd, NULL, &hIconSmall);

		HICON getIconBig = g_listBigIcon.GetAt(numHWnd);
		HICON getIconSmall = g_listSmallIcon.GetAt(numHWnd);
//*
		if (((getIconBig == NULL) || (getIconBig != hIconBig)) && (hIconBig != NULL))
		{
			DestroyIcon(getIconBig);
			HICON newIcon = icon_tools::Union(hIconBig, g_BigIcon);

			g_logHook.Save(_T("MODIFY BIG ICON"));

			if (newIcon)
			{
				if (wnd.IsWindow())
				{
					wnd.SetIcon(newIcon, 1);
					if (wnd.IsWindow())
					{
						g_listBigIcon.SetAt(numHWnd, newIcon);
						if (!hIconSmall)
						{
							wnd.SetIcon(newIcon, 0);
							g_listSmallIcon.SetAt(numHWnd, newIcon);
						}
					}
					else
					{
						DestroyIcon(newIcon);
					}
				}
				else
				{
					DestroyIcon(newIcon);
				}
			}
		}
//*/
		if (((getIconSmall == NULL) || (getIconSmall != hIconSmall)) && (hIconSmall != NULL))
		{
			DestroyIcon(getIconSmall);
			HICON newIcon = icon_tools::Union(hIconSmall, g_SmallIcon);
			g_logHook.Save(_T("MODIFY SMALL ICON"));

			if (newIcon)
			{
				if (wnd.IsWindow())
				{
					wnd.SetIcon(newIcon, 0);
					if (wnd.IsWindow())
					{
						g_listSmallIcon.SetAt(numHWnd, newIcon);
						if (!hIconBig)
						{
							wnd.SetIcon(newIcon, 1);
							g_listBigIcon.SetAt(numHWnd, newIcon);
						}
					}
					else
					{
						DestroyIcon(newIcon);
					}
				}
				else
				{
					DestroyIcon(newIcon);
				}
			}
		}

		wnd.Detach();
		if (bExtract)
		{
			DestroyIcon(hIconBig);
			DestroyIcon(hIconSmall);
		}
	}	
}

void ModifyCaptionText(CWindow wnd)
{
	if (!g_bNOCaption_ADD)
	{
		CAtlStringW str = _T("");
		CAtlStringW strCaptionAdd = _T("");
		CAtlStringW strNew = _T("");
		strCaptionAdd.LoadString(IDS_CAPTION_ADD);

		int nLength;
		LPWSTR pszText;

		if (wnd.IsWindow())
		{
			nLength = wnd.GetWindowTextLength();
			pszText = str.GetBuffer(nLength+1);
			if (wnd.IsWindow())
			{
				nLength = GetWindowTextW(wnd.m_hWnd, pszText, nLength+1);
				str.ReleaseBuffer(nLength);
				if (str.Find(strCaptionAdd) == -1)
				{
					strNew = strCaptionAdd + str;
					if (wnd.IsWindow())
						::SetWindowTextW(wnd.m_hWnd, strNew);
				}
			}
			str.ReleaseBuffer();
		}
	}
}

void VerifyIsWindow()
{	
	for (int i = 0; i < g_listInfo.GetCount(); i++)
	{		
		HOOK_ID hookId = g_listInfo.GetAt(i);
		if (!IsWindow(hookId.hWnd))
		{
			BOOL bRez = g_Hook.UnHookWnd(hookId.hWnd, hookId.hHook, TRUE);
			DWORD error = GetLastError();

			if (hookId.hProcess)
				CloseHandle(hookId.hProcess);
			if (hookId.hThread)
				CloseHandle(hookId.hThread);

			g_NumHookHWND--;
			g_logHook.Save(_T("REMOVE KILLED\t\tHWND(0x%.08X) \t\t\t\tcountHOOK(%d)\r"), (unsigned int)hookId.hWnd, g_NumHookHWND);

			HICON hIconB = g_listBigIcon.GetAt(i);
			HICON hIconS = g_listSmallIcon.GetAt(i);

			if (hIconB)
				DestroyIcon(hIconB);
			if (hIconS)
				DestroyIcon(hIconS);

			g_listBigIcon.RemoveAt(i);
			g_listSmallIcon.RemoveAt(i);			
			g_listInfo.RemoveAt(i);

			i--;
		}
	}
	if ( (g_listInfo.GetCount() == 0) && (g_listFileNotRun.GetCount() == 0) )
	{
		::SendMessage(g_hWnd, WM_DESTROY, 0, 0);
	}
}

void AddNotRunProgramm(LPTSTR lpCmdLine)
{
	PROGRAMM_INF progInf;
	_tcscpy(progInf.programRun, lpCmdLine);
	progInf.timerId = TimerID;
	if (SetTimer(g_hWnd, TimerID, 10000, NULL))
	{
		g_logHook.Save(_T("ADD NOT RUN ( %s )\r"), lpCmdLine);
		g_listFileNotRun.Add(progInf);
		TimerID ++;
	}
	if (TimerID == UINT_MAX)
		TimerID = 1;
}

void RemoveNotRunProgrammByTimer(UINT timerId)
{
	PROGRAMM_INF progInf;
	KillTimer(g_hWnd, timerId);
	for (int i = 0; i < g_listFileNotRun.GetCount(); i++)
	{				
		progInf = g_listFileNotRun.GetAt(i);
		if (progInf.timerId == timerId)
		{
			CAtlString str;
			TCHAR notRun[1024];
			if (LoadString(hInst, IDS_NOT_RUN_1, notRun, 1023) != 0)
			{
				str.Format(notRun, progInf.programRun);
				::MessageBox(g_hWnd, str, APLICATION, MB_OK | MB_ICONWARNING);
			}
			try
			{
				g_logHook.Save(_T("REMOVE NOT RUN ( %s )"), progInf.programRun);
				g_listFileNotRun.RemoveAt(i);
			}
			catch(...)
			{
				assert(!_T("erase"));
			}
			i--;
		}
	}
	if ( (g_listInfo.GetCount() == 0) && (g_listFileNotRun.GetCount() == 0) )
	{
		::SendMessage(g_hWnd, WM_DESTROY, 0, 0);
	}
	else
	{
		if (g_listInfo.GetCount() > 0)
			VerifyIsWindow();
	}
}

#include <Psapi.h>

void RemoveNotRunProgrammByHWND(HWND hWnd)
{
	if (g_listFileNotRun.GetCount() > 0)
	{
		TCHAR programRun[_MAX_DRIVE + _MAX_PATH + _MAX_FNAME + _MAX_EXT];
		ULONG dwProcessID;
		HANDLE hProcess;
//		HINSTANCE hInstance;
		BOOL bOk;

		// получаем идентификатор процесса, создавшего окно
		GetWindowThreadProcessId(hWnd, &dwProcessID);
/*
		// получаем адрес модуля в адресном пространстве того процесса
		hInstance = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
*/

		// отрываем хэндл процесса
		hProcess = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,
							FALSE, dwProcessID);
		if (hProcess == NULL)
		{
			assert(hProcess);
			return;
		}

		// получаем имя модуля в адресном пространстве процесса
		bOk = GetModuleFileNameEx(hProcess, /*hInstance*/NULL, programRun, _MAX_DRIVE + _MAX_PATH + _MAX_FNAME + _MAX_EXT);

		// закрываем хэндл процесса и выходим
		CloseHandle(hProcess);

		PROGRAMM_INF progInf;
		CAtlString strFind = _T("");
		for (int i = 0; i < g_listFileNotRun.GetCount(); i++)
		{				
			progInf = g_listFileNotRun.GetAt(i);
			strFind = progInf.programRun;
			if (strFind.Find(programRun) >= 0)
			{
				KillTimer(g_hWnd, progInf.timerId);
				try
				{
					g_logHook.Save(_T("REMOVE NOT RUN \t( %s )"), progInf.programRun);					
					g_listFileNotRun.RemoveAt(i);
				}
				catch(...)
				{
					assert(!_T("RemoveAt"));
				}
				i--;
			}
		}
	}
}

void VerifyAndHookWnd(HWND hWnd, DWORD dwProcessId)
{
	HOOK_ID hookId;
	HOOK_ID newHookId;

	TCHAR lpClassName[_MAX_PATH] = _T("");
	int nubTChar = GetClassName(hWnd, lpClassName, _MAX_PATH);
	CAtlString strFind = lpClassName;

	BOOL bFind = FALSE;
	int countNotHookClasses = sizeof(g_ListClassesNotHOOK) / sizeof(TCHAR*);
	for (int i = 0; i < countNotHookClasses; i++)
	{
		if (strFind.Find(g_ListClassesNotHOOK[i]) >=0 )
		{
			bFind = TRUE;
		}
	}
	g_logHook.Save(_T("__%s __"), lpClassName);
	if (bFind == FALSE)
	{
		for (int i = 0; i < g_listInfo.GetCount(); i++)
		{			
			hookId = g_listInfo.GetAt(i);
			if (hookId.hWnd != hWnd)
			{
				// ThreadID
				DWORD ProcessId = 0;			
				GetWindowThreadProcessId(hookId.hWnd, &ProcessId);
				if ((ProcessId == dwProcessId))
				{				
					newHookId.hWnd = hWnd;
					if ((GetParent(hWnd) == NULL) && (GetWindow(hWnd, GW_OWNER) == NULL))
					{
						CWindow wnd;
						wnd.Attach(hWnd);
//						if (wnd.IsIconic()) // wnd.IsWindowVisible() && 
						{
//						if (g_Hook.HookWnd(g_hWnd, newHookId.hWnd, newHookId.hHook))
							if (g_Hook.HookWndRet(newHookId.hWnd, newHookId.hHook, STR_HOOK_PROC_RET))
							{								
//								if (wnd.GetStyle() 
	//							wnd.SetIcon(LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON_WEB)), 0);
	//							wnd.SetIcon(LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON_WEB)), 1);
								ModifyCaptionText(wnd);								

								g_NumHookHWND++;
								g_logHook.Save(_T("ADD HOOK \t\tHWND(0x%.08X) to \tHWND(0x%.08X) \tcountHOOK(%d)"), (unsigned int)newHookId.hWnd, (unsigned int)hookId.hWnd, g_NumHookHWND);
								g_listInfo.Add(newHookId);
								g_listBigIcon.Add(NULL);
								g_listSmallIcon.Add(NULL);
								ModifyIcon(hWnd, g_listBigIcon.GetCount() - 1);
								RemoveNotRunProgrammByHWND(newHookId.hWnd);
							}
							else
							{
								assert("HookThreadId FALSE");
							}
						}
						wnd.Detach();
						break;
					}
				}
			}
		}
	}
	if ( (g_listInfo.GetCount() == 0) && (g_listFileNotRun.GetCount() == 0) )
	{
		::SendMessage(g_hWnd, WM_DESTROY, 0, 0);
	}
	else
	{
		if (g_listInfo.GetCount() > 0)
			VerifyIsWindow();
	}
}

BOOL CALLBACK EnumThreadWndProc(HWND hwnd, LPARAM lParam)
{
    WINDOWINFO wi;
    if (IsWindow(hwnd) && GetWindowInfo(hwnd,&wi))
    {
        DWORD dwS = wi.dwStyle ;
        if ( (dwS & (WS_VISIBLE | WS_CAPTION | WS_OVERLAPPED)) == 
            (WS_VISIBLE | WS_CAPTION | WS_OVERLAPPED)
             && ((dwS & WS_CHILDWINDOW) != WS_CHILDWINDOW)) 
        {		
			g_FindHWnd = hwnd;
			return FALSE;
        }
    }
	return TRUE;
}

BOOL CALLBACK EnumWndProc(HWND hwnd, LPARAM lParam)
{
    WINDOWINFO wi;
    if (IsWindow(hwnd) && GetWindowInfo(hwnd,&wi))
    {
        DWORD dwS = wi.dwStyle ;
        if ( (dwS & (WS_VISIBLE | WS_CAPTION | WS_OVERLAPPED)) == 
            (WS_VISIBLE | WS_CAPTION | WS_OVERLAPPED)
             && ((dwS & WS_CHILDWINDOW) != WS_CHILDWINDOW)) 
        {
			DWORD ProcessId = GetWindowThreadProcessId(hwnd, NULL);
			if ((DWORD)lParam == ProcessId)
			g_FindHWnd = hwnd;
			return FALSE;
        }
    }
	return TRUE;
}

void UnHookAll()
{
	HOOK_ID hookId;
	for (int i = g_listInfo.GetCount() - 1; i >= 0; i--)
	{
		hookId = g_listInfo.GetAt(i);
		BOOL bRez = g_Hook.UnHookWnd(hookId.hWnd, hookId.hHook);
		DWORD error = GetLastError();
		g_logHook.Save(_T("UnHookWnd(hookId.hWnd, hookId.hHook) -> BOOL rez = %d; ERROR rez = %d"), bRez, error);
	}
	g_listInfo.RemoveAll();

	for (i = g_listBigIcon.GetCount() - 1; i >= 0; i--)
	{
		HICON hIconB = g_listBigIcon.GetAt(i);
		HICON hIconS = g_listSmallIcon.GetAt(i);
		DestroyIcon(hIconB);
		DestroyIcon(hIconS);
	}
	g_listBigIcon.RemoveAll();
	g_listSmallIcon.RemoveAll();

	if (gHookCBT != NULL)
	{
		BOOL bRez = g_Hook.UnHookWnd(NULL, gHookCBT);
		DWORD error = GetLastError();
		if (bRez)
			gHookCBT = NULL;
		g_logHook.Save(_T("UnHookWnd(NULL, gHookCBT) -> BOOL rez = %d; ERROR rez = %d"), bRez, error);
	}
}

void RunProgramm(LPTSTR lpCmdLine)
{
	DWORD dwTheadID = NULL;
	TCHAR strError[MAX_LENGTH_ERROR] = _T("");
	HANDLE hProcess = NULL;
	HANDLE hThread = NULL;
	BOOL bRun = TRUE;
	DWORD dRes = Restricting(NULL, lpCmdLine, strError, sizeof strError, dwTheadID, hProcess, hThread);
	if (dRes != ERROR_SUCCESS)
	{
		g_log.Save(_T("NO Run restricted -> %s"), lpCmdLine);

		CAtlString strVerify = lpCmdLine;
		CAtlString strTempCmd = lpCmdLine;
		strVerify.MakeUpper();
		int posIE = -1;
		if ((posIE = strVerify.Find(STR_IEXPLORE_EXE_2)) >= 0)
		{
			strVerify = strTempCmd.Mid(posIE + _tcsclen(STR_IEXPLORE_EXE_2));
		}
		else
		{
			if ((posIE = strVerify.Find(STR_IEXPLORE_EXE_1)) >= 0)
			{
				strVerify = strTempCmd.Mid(posIE + _tcsclen(STR_IEXPLORE_EXE_1));				
			}
			else
			{
				strVerify = strTempCmd;
			}
		}
		strVerify.TrimLeft();

		CAtlString str = _T("");
		DB_DATA DBData;
		DBData.lpRegSaveName = APLICATION;
		CDefaultBrowser defBrow;
		if (defBrow.GetOldBrowserFullFileName(APLICATION, str) || defBrow.FindFileNameByFile(str))
		{
			str = str + STR_SPACE + strVerify;
			DWORD dRes = Restricting(NULL, str.GetBuffer(), strError, sizeof strError, dwTheadID, hProcess, hThread);
			str.ReleaseBuffer();
			if (dRes != ERROR_SUCCESS)
			{
				bRun = FALSE;
				CAtlString strErr = _T("");
				CAtlString strMask = _T("");
				strMask.LoadString(IDS_NOT_RUN_2);
				if (!strMask.IsEmpty())
				{
					strErr.Format(strMask, lpCmdLine);
					::MessageBox(g_hWnd, strErr, APLICATION, MB_OK | MB_ICONWARNING);
				}
				assert(_tprintf(MSG_ERROR, strError, dRes) != -1);
			}
			else
			{
				g_log.Save(_T("Run old browser restricted -> %s"), str);
			}
		}
		else
		{
			CAtlString strErr = _T("");
			CAtlString strMask = _T("");
			bRun = FALSE;			
			strMask.LoadString(IDS_NOT_RUN_2);
			if (!strMask.IsEmpty())
			{
				str.Format(strMask, lpCmdLine);
				::MessageBox(g_hWnd, strErr, APLICATION, MB_OK | MB_ICONWARNING);
			}
			assert(_tprintf(MSG_ERROR, strError, dRes) != -1);
		}
	}
	else
	{
		g_log.Save(_T("Run restricted -> %s"), lpCmdLine);
	}
	if (bRun)
	{		
		HOOK_ID hookId;
		hookId.hWnd = NULL;
		g_FindHWnd = NULL;
		BOOL bProcess = TRUE;
		DWORD timeOut = GetTickCount();
		while ((GetTickCount() - timeOut) < (60 * 1000))
		{
			BOOL bRez = ::EnumThreadWindows(dwTheadID, (WNDENUMPROC)EnumThreadWndProc, 0);
//			BOOL bRez = EnumWindows(EnumWndProc, (LPARAM)dwTheadID);
			hookId.hWnd = g_FindHWnd;
			if (g_FindHWnd)
				break;
			if (WaitForSingleObject(hProcess, 10) != WAIT_TIMEOUT)
			{
				bProcess = FALSE;
				break;
			}
		}
		if ( (!g_FindHWnd) && (bProcess) )
		{
			CloseHandle(hProcess);
			CloseHandle(hThread);
			CAtlString strErr = _T("");
			CAtlString strMask = _T("");
			strMask.LoadString(IDS_NOT_RUN_1);
			if (!strMask.IsEmpty())
			{
				strErr.Format(strMask, lpCmdLine);
				::MessageBox(g_hWnd, strErr, APLICATION, MB_OK | MB_ICONWARNING);
			}
			assert(!"EnumThreadWindows == NULL");
			if ( (g_listInfo.GetCount() == 0) && (g_listFileNotRun.GetCount() == 0) )
			{
				::SendMessage(g_hWnd, WM_DESTROY, 0, 0);
			}
			else
			{
				if (g_listInfo.GetCount() > 0)
					VerifyIsWindow();
			}
			return;
		}
		else
		{			
			if (!bProcess)
			{
				g_log.Save(_T("NO bProcess -> %s"), lpCmdLine);
				AddNotRunProgramm(lpCmdLine);
				return;
			}
		}
		if (g_FindHWnd)
		{
			hookId.hProcess = hProcess;
			hookId.hThread = hThread;
			if (g_Hook.HookWndRet(hookId.hWnd, hookId.hHook, STR_HOOK_PROC_RET))
			{
				CAtlString str;
				CAtlString strNew;
				CWindow wnd;
				wnd.Attach(g_FindHWnd);
				ModifyCaptionText(wnd);
				wnd.Detach();

				g_NumHookHWND++;
				g_logHook.Save(_T("HOOK NEW \t\tHWND(0x%.08X) \t\t\t\tcountHOOK(%d)"), (unsigned int)hookId.hWnd, g_NumHookHWND);
				g_listInfo.Add(hookId);
				g_listBigIcon.Add(NULL);
				g_listSmallIcon.Add(NULL);

				ModifyIcon(g_FindHWnd, g_listBigIcon.GetCount() - 1);				

				if (gHookCBT == NULL)
				{
					if (!g_Hook.HookCBT(NULL, gHookCBT, STR_HOOK_CBT_NAME))
					{
						g_logHook.Save(_T("gHookCBT NULL"));
					}
					else
					{
						HWND hookData[2] = {0};
						hookData[0] = g_hWnd;
						hookData[1] = (HWND)gHookCBT;
						g_MFile.AddData((const BYTE*)&hookData, sizeof hookData);
					}
				}
			}
			else
			{
				assert("HookThreadId FALSE");
			}
		}
		else
		{
			g_log.Save(_T("NO find HANDLE -> %s"), lpCmdLine);
		}
	}
	if ( (g_listInfo.GetCount() == 0) && (g_listFileNotRun.GetCount() == 0) )
	{		
		::SendMessage(g_hWnd, WM_DESTROY, 0, 0);
	}
	else
	{		
		if (g_listInfo.GetCount() > 0)
			VerifyIsWindow();
	}
}

void EndProgramm(HWND hWnd)
{
	HOOK_ID hookId;
	for (int i = 0; i < g_listInfo.GetCount(); i++)
	{
		hookId = g_listInfo.GetAt(i);
		if (hookId.hWnd == hWnd)
		{		
			BOOL bRez = g_Hook.UnHookWnd(hookId.hWnd, hookId.hHook, TRUE);
			DWORD error = GetLastError();

			if (hookId.hProcess)
				CloseHandle(hookId.hProcess);
			if (hookId.hThread)
				CloseHandle(hookId.hThread);

			g_NumHookHWND--;
			g_logHook.Save(_T("REMOVE END PROGRAMM \tHWND(0x%.08X) \t\t\t\tcountHOOK(%d)"), (unsigned int)hookId.hWnd, g_NumHookHWND);
			g_logHook.Save(_T("UnHookWnd(hookId.hWnd, hookId.hHook, TRUE) -> BOOL rez = %d; ERROR rez = %d"), bRez, error);

			HICON hIconB = g_listBigIcon.GetAt(i);
			HICON hIconS = g_listSmallIcon.GetAt(i);
			DestroyIcon(hIconB);
			DestroyIcon(hIconS);
			g_listBigIcon.RemoveAt(i);
			g_listSmallIcon.RemoveAt(i);
			g_listInfo.RemoveAt(i);

			i--;
			break;
		}
	}
	if ( (g_listInfo.GetCount() == 0) && (g_listFileNotRun.GetCount() == 0) )
	{
		::SendMessage(g_hWnd, WM_DESTROY, 0, 0);
	}
	else
	{
		if (g_listInfo.GetCount() > 0)
			VerifyIsWindow();
	}
}

BOOL GetKeyData(LPCTSTR lpKeyNamem, DWORD &dwKey)
{
	HKEY hKey = NULL;
	DWORD dwSize = sizeof(dwKey);
	CAtlString strSubKey = _T("");
	strSubKey.Format(STR_PROGRAMM_SAVE_FOLDER_MAIN, APLICATION);
	if (RegOpenKeyEx(HKEY_CURRENT_USER, strSubKey, 0, KEY_QUERY_VALUE | KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		if (RegQueryValueEx(hKey, lpKeyNamem, NULL, NULL, (LPBYTE)&dwKey, &dwSize) == ERROR_MORE_DATA)
		{
			return TRUE;
		}
		RegCloseKey(hKey);
	}
	return FALSE;
}

BOOL SetKeyData(LPTSTR lpKeyNamem, DWORD dwKey)
{
	HKEY hKey = NULL;
	DWORD dwSize = sizeof(dwKey);
	CAtlString strSubKey = _T("");
	strSubKey.Format(STR_PROGRAMM_SAVE_FOLDER_MAIN, APLICATION);
	if(::RegCreateKeyEx(HKEY_CURRENT_USER, strSubKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, 0) == ERROR_SUCCESS)
	{
		if (::RegSetValueEx(hKey, lpKeyNamem, 0, REG_DWORD, (LPBYTE)&dwKey, dwSize) == ERROR_SUCCESS)
		{
			return TRUE;
		}
		RegCloseKey(hKey);
	}
	return FALSE;
}

BOOL IsPress(BYTE nKey, BYTE nKeyID, DWORD dwKey, int num)
{
	BYTE numKey = (BYTE)(dwKey >> (8 * num));
	if ((numKey != 0) && (numKey == nKeyID))
	{
		if ((nKey & 0x80) == 0x80)
			return TRUE;
	}
	if ((numKey == 0) && ((nKey == 0) || (nKey == 0x01)))
		return TRUE;
	return FALSE;
}

BOOL IsKeyPressed(DWORD dwKey)
{
	if (dwKey != 0)
	{
		BYTE nKeyList[256];
		if (GetKeyboardState(nKeyList))
		{
			if (IsPress(nKeyList[VK_MENU], VK_MENU, dwKey, 0) &&
				IsPress(nKeyList[VK_CONTROL], VK_CONTROL, dwKey, 1) &&
				IsPress(nKeyList[VK_SHIFT], VK_SHIFT, dwKey, 2))
				return TRUE;
		}
	}
/*
	if (nKey != 0)
	{
		if ((nKey >= 0) && (nKey <= 255))
		{
			BYTE nKeyList[256];
			if (GetKeyboardState(nKeyList))
			{
				if ((nKey == VK_NUMLOCK) || (nKey == VK_SCROLL) || (nKey == VK_CAPITAL))
				{
					if ((nKeyList[nKey] & 0x01) == 0x01)
						return TRUE;
				}
				else
				{
					if ((nKeyList[nKey] & 0x80) == 0x80)
						return TRUE;
				}
			}
		}
	}
*/
	return FALSE;
}

BOOL SetAsDefault()
{
	CDefaultBrowser defBrow;
	DB_DATA DBData;
	DBData.nTypeSet = HKEY_CURRENT_USER; // default
	DBData.lpRegSaveName = APLICATION;
	TCHAR fileName[_MAX_PATH + 1] = _T("");
	GetModuleFileName((HMODULE)hInst, fileName, _MAX_PATH);
	DBData.lpFullFileName = fileName;
	CAtlString strFullName = DBData.lpFullFileName;
	int pos = strFullName.Find(PathFindFileName(DBData.lpFullFileName));	
	strFullName = strFullName.Mid(0, pos) + _T("ErrorBrowser.log");
	defBrow.SetLogFileName(strFullName.GetBuffer());
	strFullName.ReleaseBuffer();
//	BOOL bBrow = TRUE;
//	defBrow.GetErrorInfo(DBData, bBrow);
	DeleteFile(strFullName);
	
	if (defBrow.IsSetDef(DBData))
	{
		return FALSE;
	}
	else
	{
		DB_DATA verify;
		verify.lpFullFileName = DBData.lpFullFileName;
		verify.lpRegSaveName = DBData.lpRegSaveName;
		verify.nTypeSet = HKEY_CURRENT_USER;
		BOOL bBrow = TRUE;
		BOOL bRezz = defBrow.IsPressentBrowser(verify, bBrow);
		defBrow.GetErrorInfo(verify, bBrow);

		verify.nTypeSet = HKEY_LOCAL_MACHINE;

		if ((!bRezz) && (!defBrow.IsPressentBrowser(verify, bBrow)))
		{
			defBrow.GetErrorInfo(verify, bBrow);
			defBrow.Set(DBData);
////////////////////////////////////////////////////
//			Windows 2000
//
//			
			OSVERSIONINFO verInf;
			verInf.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
			if (::GetVersionEx(&verInf))
			{
				if ( (verInf.dwMajorVersion == 5) && (verInf.dwPlatformId == VER_PLATFORM_WIN32_NT) )
				{
					if (verInf.dwMinorVersion == 0)
					{
						DBData.nTypeSet = HKEY_LOCAL_MACHINE;
						defBrow.Set(DBData);
					}
				}
			}
//
////////////////////////////////////////////////////

			DWORD nKey = 0;
			GetKeyData(STR_KEY, nKey);
			if (nKey == 0)
			{
				nKey = (nKey << 8) + (BYTE)VK_SHIFT;
				nKey = (nKey << 8) + (BYTE)0x00;
				nKey = (nKey << 8) + (BYTE)0x00;
				SetKeyData(STR_KEY, nKey);
			}
			return FALSE;
		}
		else
		{
			defBrow.GetErrorInfo(verify, bBrow);
		}
	}
	return TRUE;
}

BOOL RestoreOldDefault()
{
	CDefaultBrowser defBrow;
	DB_DATA DBData;
	DBData.nTypeSet = HKEY_CURRENT_USER; // default
	DBData.lpRegSaveName = APLICATION;
	TCHAR fileName[_MAX_PATH + 1] = _T("");
	GetModuleFileName((HMODULE)hInst, fileName, _MAX_PATH);
	DBData.lpFullFileName = fileName;
/*
	CAtlString strFullName = DBData.lpFullFileName;
	int pos = strFullName.Find(PathFindFileName(DBData.lpFullFileName));
	strFullName = strFullName.Mid(0, pos) + _T("ErrorBrowser.log");
	defBrow.SetLogFileName(strFullName.GetBuffer());
*/	
	if (defBrow.IsSetDef(DBData))
	{
		defBrow.Restore(DBData);
////////////////////////////////////////////////////
//			Windows 2000
//
//			
			OSVERSIONINFO verInf;
			verInf.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
			if (::GetVersionEx(&verInf))
			{
				if ( (verInf.dwMajorVersion == 5) && (verInf.dwPlatformId == VER_PLATFORM_WIN32_NT) )
				{
					if (verInf.dwMinorVersion == 0)
					{
						DBData.nTypeSet = HKEY_LOCAL_MACHINE;
						defBrow.Restore(DBData);
					}
				}
			}
//
////////////////////////////////////////////////////
		return FALSE;
	}
	return TRUE;
}

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	hInst = hInstance;
	TCHAR fileName[_MAX_PATH + 1] = _T("");
	GetModuleFileName((HMODULE)hInst, fileName, _MAX_PATH + 1);
	CAtlString strFullName = fileName;
#ifdef _DEBUG
/*
	CAtlString strLine = lpCmdLine;
	CAtlString strSize;
	strSize.Format(_T("Size = %d"), strLine.GetLength());
	MessageBox(NULL, lpCmdLine, strSize.GetBuffer(), MB_OK);	
*/
//	CAtlString strFullName = fileName;
	int pos = strFullName.Find(PathFindFileName(fileName));
	strFullName = strFullName.Mid(0, pos) + _T("main.log");
	g_log.Init(strFullName.GetBuffer());
	strFullName = fileName;
	pos = strFullName.Find(PathFindFileName(fileName));
	strFullName = strFullName.Mid(0, pos) + _T("HookWnd.log");
	g_logHook.Init(strFullName.GetBuffer());
#endif	
	g_log.Save(_T("    "));
	g_log.Save(_T("_tWinMain -> %s"), lpCmdLine);

	MSG msg;
	HACCEL hAccelTable;
	
	if (LoadString(hInst, IDS_APLICATION, APLICATION, MAX_LOADSTRING) == 0)
	{
		CAtlString strApp = PathFindFileName(fileName);
		int pos = strApp.Find(STR_POINT);
		if (pos >= 0)
			strApp = strApp.Mid(0, pos);
		_tcscpy(APLICATION, strApp);
	}

	DWORD nKeyLog = 0;
	GetKeyData(STR_LOG, nKeyLog);
	if (nKeyLog == 1)
	{
		strFullName = fileName;
		int pos2 = strFullName.Find(PathFindFileName(fileName));		
		strFullName = strFullName.Mid(0, pos2) + _T("HookWnd.log");
		g_logHook.Init(strFullName.GetBuffer());
	}

	CAtlString strFind = _T("");

	g_strLaunch = lpCmdLine;
	if (_tcscmp(lpCmdLine, _T("")) == 0)
	{
		if (DialogBox(hInstance, (LPCTSTR)IDD_DIALOG_OPTION, HWND_DESKTOP, (DLGPROC)OptionProc) != 0)
		{
			return FALSE;
		}
		else
		{			
			CDefaultBrowser defBrow;
			defBrow.FindFileNameByFile(strFind);
			strFind += _T(" ");
			g_strLaunch = strFind + g_strLaunch;
		}			
	}

	CAtlString findOption = lpCmdLine;
	findOption.MakeUpper();
	findOption += STR_SPACE;
	if (findOption.Find(STR_INSTALL) >= 0)
	{
		return SetAsDefault();
	}
	if (findOption.Find(STR_UNINSTALL) >= 0)
	{
		return RestoreOldDefault();
	}
	BOOL bLeave = FALSE;
//*
	if (findOption.Find(STR_EMBEDDING) >= 0)
	{
//		bLeave = TRUE;
		g_bEmbedding = TRUE;
	}
//*/
	BOOL bNOSplash = FALSE;
	int posSP = -1;
	if ((posSP = findOption.Find(STR_NOSPLASH)) >= 0)
	{
		findOption = findOption.Mid(0, posSP) + findOption.Mid(posSP + _tcslen(STR_NOSPLASH));
		g_strLaunch = g_strLaunch.Mid(0, posSP) + g_strLaunch.Mid(posSP + _tcslen(STR_NOSPLASH));
		bNOSplash = TRUE;
	}
	posSP = -1;
	if ((posSP = findOption.Find(STR_NOCAPTION_ADD)) >= 0)
	{
		findOption = findOption.Mid(0, posSP) + findOption.Mid(posSP + _tcslen(STR_NOCAPTION_ADD));
		g_strLaunch = g_strLaunch.Mid(0, posSP) + g_strLaunch.Mid(posSP + _tcslen(STR_NOCAPTION_ADD));
		g_bNOCaption_ADD = TRUE;
	}	

	DWORD nKey = 0;
	GetKeyData(STR_KEY, nKey);
	BOOL bKeyPressed = IsKeyPressed(nKey);

	g_bIsSecuriRun = TRUE;

	if ((strFind.IsEmpty()) && (!bNOSplash))
	{
		if (!bKeyPressed)
		{
			DWORD dwShow = SW_SHOW;//SW_HIDE
			GetKeyData(STR_DLGRUNSHOW, dwShow);
			if (dwShow == SW_SHOW)
			{
				if (!bLeave)
				{
					INT_PTR ptrRez = 0;
//					ptrRez = DialogBox(hInstance, (LPCTSTR)IDD_DIALOG_RUN_MODE, NULL, (DLGPROC)DlgRunProc);
					ptrRez = DialogBox(hInstance, (LPCTSTR)IDD_DIALOG_RUN, NULL, (DLGPROC)DlgRunProcNew);
					if (ptrRez == IDCANCEL)
						return FALSE;
				}
			}
		}
	}

	if (bKeyPressed)
	{
		g_bIsSecuriRun = FALSE;
	}

	if (!g_bIsSecuriRun)
	{
		g_log.Save(_T("Run no restricted -> %s"), g_strLaunch);
		STARTUPINFO si = { sizeof si };
		PROCESS_INFORMATION pi;
		GetStartupInfo(&si);
		if (!CreateProcess(NULL, g_strLaunch.GetBuffer(), 0, 0, FALSE, CREATE_NEW_CONSOLE, 0, 0, &si, &pi ))
		{
			g_log.Save(_T("Run -> %s"), g_strLaunch);
			CAtlString str = _T("");
//			DB_DATA DBData;
//			DBData.lpRegSaveName = APLICATION;
			CDefaultBrowser defBrow;
			if (defBrow.GetOldBrowserFullFileName(APLICATION, str))
			{
				str = str + STR_SPACE + g_strLaunch;
				if (!CreateProcess(NULL, str.GetBuffer(), 0, 0, FALSE, CREATE_NEW_CONSOLE, 0, 0, &si, &pi ))
				{
					TCHAR notRun[1024];
					if (LoadString(hInst, IDS_NOT_RUN_2, notRun, 1023) != 0)
					{
						str.Format(notRun, g_strLaunch);
						::MessageBox(g_hWnd, str, APLICATION, MB_OK | MB_ICONWARNING);
					}
					g_log.Save(_T("Run old browser restricted -> %s"), str);
				}
			}
			else
			{
				TCHAR notRun[1024];
				if (LoadString(hInst, IDS_NOT_RUN_2, notRun, 1023) != 0)
				{
					str.Format(notRun, lpCmdLine);
					::MessageBox(g_hWnd, str, APLICATION, MB_OK | MB_ICONWARNING);
				}
			}
		}
		return FALSE;
	}
	
	try
	{
		// Initialize global strings
		LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
		LoadString(hInstance, IDC_SECURITYRUN, szWindowClass, MAX_LOADSTRING);
	}
	catch(...)
	{
		assert(!"LoadString");
	}
	if (MyRegisterClass(hInstance) == 0)
		MessageBox(NULL, _T("Error"), _T(""), MB_OK);
//	assert(MyRegisterClass(hInstance));
	
	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow, g_strLaunch.GetBuffer())) 
	{
		return FALSE;
	}

	if (!strFullName.IsEmpty())
		DeleteFile(strFullName);
	g_logHook.Save(_T(":__NEW SESSION________________>\r"));

	g_BigIcon = (HICON)LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON_LOCK), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	g_SmallIcon = (HICON)LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON_LOCK), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	assert(g_BigIcon);
	assert(g_SmallIcon);

	DeletePermission();	

	RunProgramm(g_strLaunch.GetBuffer());	

	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_SECURITYRUN);

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_SECURITYRUN);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= NULL;//*(LPCTSTR)IDC_SECURITYRUN;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	return RegisterClassEx(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow, LPTSTR lpCmdLine)
{
   HWND hWnd = NULL;

   hInst = hInstance; // Store instance handle in our global variable


   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      /*CW_USEDEFAULT*/4096, 0, /*CW_USEDEFAULT*/0, 0, NULL, NULL, hInstance, NULL);

//	ShowWindow(hWnd, nCmdShow);
	ShowWindow(hWnd, SW_HIDE);
	UpdateWindow(hWnd);

   if (!hWnd)
   {
	  MessageBox(NULL, _T("NOT Create Windows"), _T(""), MB_OK);
      return FALSE;
   }
   else
   {
		HWND hookData[2] = {0};
		HWND hwnd;
//		if (g_MFile.Open(sizeof HWND))
		if (g_MFile.Open(sizeof hookData))
		{
//			g_MFile.GetData((BYTE*)&hwnd, sizeof HWND);
//			::SetWindowText(hwnd, lpCmdLine);
//			::SendMessage(hwnd, WM_RUN_NEW_PROGRAMM, 0, 0);
			g_MFile.GetData((BYTE*)&hookData, sizeof hookData);
			::SetWindowText(hookData[0], lpCmdLine);
			::SendMessage(hookData[0], WM_RUN_NEW_PROGRAMM, 0, 0);
			return FALSE;
		}
		else
		{
//			if (g_MFile.Create(sizeof HWND))
			if (g_MFile.Create(sizeof hookData))
			{
				hookData[0] = hWnd;
//				g_MFile.AddData((const BYTE*)&hWnd, sizeof HWND);
				g_MFile.AddData((const BYTE*)&hookData, sizeof hookData);
//				assert(!"NO Creating Mapping File !!!!!");HHOOK
			}
			else
			{
				assert(!"NO Creating Mapping File !!!!!");
				return FALSE;
			}
		}
   }

//   ShowWindow(hWnd, nCmdShow);
   g_hWnd = hWnd;

   return TRUE;
}

void verifyIcon(HWND hWnd)
{
	HOOK_ID hookId;
	for (int i = 0; i < g_listInfo.GetCount(); i++)
	{		
		hookId = g_listInfo.GetAt(i);
		if (hookId.hWnd == hWnd)
		{
			ModifyIcon(hWnd, i);
		}
	}
}
void verifyText(HWND hWnd)
{
	HOOK_ID hookId;
	for (int i = 0; i < g_listInfo.GetCount(); i++)
	{		
		hookId = g_listInfo.GetAt(i);
		if (hookId.hWnd == hWnd)
		{
			CWindow wnd;
			wnd.Attach(hWnd);
			if (wnd.IsWindow())
			{
				ModifyCaptionText(wnd);	
			}
			wnd.Detach();
		}
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
//	PAINTSTRUCT ps;
//	HDC hdc;

	if (message == WM_RUN_NEW_PROGRAMM)
	{
		CAtlString strFileName;
		CWindow wnd;
		wnd.Attach(hWnd);
		wnd.GetWindowText(strFileName);
		g_log.Save(_T("WM_RUN_NEW_PROGRAMM -> %s"), strFileName);
		RunProgramm(strFileName.GetBuffer());
		strFileName.ReleaseBuffer();
		wnd.Detach();
		return 1;
	}

	if (message == WM_CREATE_WND)
	{
		g_logHook.Save(_T("__ NEW WND __ (0x%.08X)"), (unsigned int)(HWND)wParam);
		VerifyAndHookWnd((HWND)wParam, (DWORD)lParam);
		return 0;
	}

	if (message == WM_GET_MSG_WND)
	{
		switch (wParam)
		{
		case 0:
			verifyText((HWND)lParam);
			return 0;
		case 1:
			verifyIcon((HWND)lParam);
			return 0;
		case 2:
			EndProgramm((HWND)lParam);
			return 0;
		};
	}

	switch (message) 
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam); 
		wmEvent = HIWORD(wParam); 
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
/*
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;
*/
	case WM_TIMER :
		RemoveNotRunProgrammByTimer((UINT)wParam);
		break;
	case WM_DESTROY:
		{
			UnHookAll();
			g_MFile.Close();
			RestorePermission();
			PostQuitMessage(0);
		}
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}	

	return 0;
}

// Message handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{
			::SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)(HICON)LoadIcon(hInst, (LPCTSTR)IDI_SECURITYRUN));
		}
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void InitData(HWND hDlg)
{
	::SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)(HICON)LoadIcon(hInst, (LPCTSTR)IDI_ICON_CONFIG));

	DB_DATA DBData;
	DBData.nTypeSet = HKEY_CURRENT_USER; // default
	DBData.lpRegSaveName = APLICATION;
	TCHAR fileName[_MAX_PATH + 1] = _T("");
	GetModuleFileName((HMODULE)hInst, fileName, _MAX_PATH);
	DBData.lpFullFileName = fileName;
	CAtlString strFullName = DBData.lpFullFileName;
	int pos = strFullName.Find(PathFindFileName(DBData.lpFullFileName));	
	strFullName = strFullName.Mid(0, pos) + _T("ErrorBrowser.log");
	CDefaultBrowser defBrow;
	BOOL bBrow = TRUE;
	defBrow.SetLogFileName(strFullName.GetBuffer());	
	if (defBrow.IsSetDef(DBData))
	{
		if (defBrow.IsModifyBrowser(DBData))
		{
			defBrow.GetErrorInfo(DBData, bBrow);
			if (DialogBox(hInst, (LPCTSTR)IDD_DIALOG_SET_AS_DEFAULT, hDlg, (DLGPROC)DlgSetAsDefaultProc) == IDOK)
			{
				defBrow.Modify(DBData);
////////////////////////////////////////////////////
//			Windows 2000
//
//			
			OSVERSIONINFO verInf;
			verInf.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
			if (::GetVersionEx(&verInf))
			{
				if ( (verInf.dwMajorVersion == 5) && (verInf.dwPlatformId == VER_PLATFORM_WIN32_NT) )
				{
					if (verInf.dwMinorVersion == 0)
					{
						DBData.nTypeSet = HKEY_LOCAL_MACHINE;
						defBrow.Modify(DBData);
					}
				}
			}
//
////////////////////////////////////////////////////
			}
		}
	}
	else
	{
/*
DB_DATA verify;
		verify.lpFullFileName = DBData.lpFullFileName;
		verify.lpRegSaveName = DBData.lpRegSaveName;
		verify.nTypeSet = HKEY_CURRENT_USER;
		BOOL bBrow = TRUE;
		BOOL bRezz = defBrow.IsPressentBrowser(verify, bBrow);
		defBrow.GetErrorInfo(verify, bBrow);

		verify.nTypeSet = HKEY_LOCAL_MACHINE;

		if ((!bRezz) && (!defBrow.IsPressentBrowser(verify, bBrow)))
		{
*/
		BOOL bBrow = TRUE;
		BOOL bRezz = defBrow.IsPressentBrowser(DBData, bBrow);
		defBrow.GetErrorInfo(DBData, bBrow);

		DBData.nTypeSet = HKEY_LOCAL_MACHINE;

		if ((!bRezz) && (!defBrow.IsPressentBrowser(DBData, bBrow)))
		{
			defBrow.GetErrorInfo(DBData, bBrow);
			if (DialogBox(hInst, (LPCTSTR)IDD_DIALOG_SET_AS_DEFAULT, hDlg, (DLGPROC)DlgSetAsDefaultProc) == IDOK)
			{
//				defBrow.GetErrorInfo(DBData, bBrow);
				DBData.nTypeSet = HKEY_CURRENT_USER; // default				
				defBrow.Set(DBData);
////////////////////////////////////////////////////
//			Windows 2000
//
//			
			OSVERSIONINFO verInf;
			verInf.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
			if (::GetVersionEx(&verInf))
			{
				if ( (verInf.dwMajorVersion == 5) && (verInf.dwPlatformId == VER_PLATFORM_WIN32_NT) )
				{
					if (verInf.dwMinorVersion == 0)
					{
						DBData.nTypeSet = HKEY_LOCAL_MACHINE;
						defBrow.Set(DBData);
					}
				}
			}
//
////////////////////////////////////////////////////
			}
		}
		else
		{
			defBrow.GetErrorInfo(DBData, bBrow);
			TCHAR buff[1024];
			TCHAR buff2[1024];
			LoadString(hInst, IDS_SET_BROWSER, buff, 1023);
			LoadString(hInst, IDS_WARNING, buff2, 1023);			
			MessageBox(NULL, buff, buff2, MB_OK);
		}
	}
	
	DWORD dwData = SW_SHOW;
	GetKeyData(STR_DLGRUNSHOW, dwData);
	if (dwData == SW_SHOW)
	{
		CheckDlgButton(hDlg, IDC_CHECK_DLG_RUN_SHOW, BST_CHECKED);
	}
	SendMessage(hDlg, WM_COMMAND, MAKELPARAM(IDC_CHECK_DLG_RUN_SHOW, 0), 0);

	HWND hEdit = GetDlgItem(hDlg, IDC_EDIT_WILE_SECOND);
	SendMessage(hEdit, EM_LIMITTEXT, MAX_NUM_DIGITS, 0L);

	dwData = MIN_WHILE_VALUE;
	GetKeyData(STR_DLGWHILESHOW, dwData);
	if (dwData == 0) 
		dwData = MIN_WHILE_VALUE;
	TCHAR num[MAX_NUM_DIGITS + 1];
	_itot(dwData, num, 10);
	::SetWindowText(hEdit, num);

	DWORD nKey = 0;
	GetKeyData(STR_KEY, nKey);
/*
	if (nKey == 0)
	{
		nKey = (nKey << 8) + (BYTE)VK_SHIFT;
		nKey = (nKey << 8) + (BYTE)0x00;
		nKey = (nKey << 8) + (BYTE)0x00;
		SetKeyData(STR_KEY, nKey);
	}
*/
	if (VK_MENU == (int)(byte)nKey)
	{
		CheckDlgButton(hDlg, IDC_RADIO_KEY_ALT, BST_CHECKED);
	}
	nKey = nKey >> 8;
	if (VK_CONTROL == (int)(byte)nKey)
	{
		CheckDlgButton(hDlg, IDC_RADIO_KEY_CTRL, BST_CHECKED);
	}
	nKey = nKey >> 8;
	if (VK_SHIFT == (int)(byte)nKey)
	{
		CheckDlgButton(hDlg, IDC_RADIO_KEY_SHIFT, BST_CHECKED);
	}
}

BOOL SaveData(HWND hDlg)
{
	DWORD rezKey = 0;
	if (IsDlgButtonChecked(hDlg, IDC_RADIO_KEY_SHIFT)) 
		rezKey = (rezKey << 8) + (BYTE)VK_SHIFT;
	else
		rezKey = (rezKey << 8) + (BYTE)0x00;
	if (IsDlgButtonChecked(hDlg, IDC_RADIO_KEY_CTRL)) 
		rezKey = (rezKey << 8) + (BYTE)VK_CONTROL;
	else
		rezKey = (rezKey << 8) + (BYTE)0x00;
	if (IsDlgButtonChecked(hDlg, IDC_RADIO_KEY_ALT)) 
		rezKey = (rezKey << 8) + (BYTE)VK_MENU;
	else
		rezKey = (rezKey << 8) + (BYTE)0x00;

	SetKeyData(STR_KEY, rezKey);

	HWND hEdit = GetDlgItem(hDlg, IDC_EDIT_WILE_SECOND);
	TCHAR num[MAX_NUM_DIGITS + 1];
	::GetWindowText(hEdit, num, MAX_NUM_DIGITS + 1);
	SetKeyData(STR_DLGWHILESHOW, _tstol(num));

	if (IsDlgButtonChecked(hDlg, IDC_CHECK_DLG_RUN_SHOW))
		SetKeyData(STR_DLGRUNSHOW, SW_SHOW);
	else
		SetKeyData(STR_DLGRUNSHOW, SW_HIDE);

	return TRUE;
}

HIMAGELIST m_ImageList = NULL;
CHyperLink hLihk;

SIZE GetFrameSize(HWND wnd)
{
	SIZE sFrameSize = {NULL};
	LONG wndStyle = GetWindowLong(wnd, GWL_STYLE);
	if(wndStyle & WS_THICKFRAME)
	{
		sFrameSize.cx = GetSystemMetrics(SM_CXSIZEFRAME);
		sFrameSize.cy = GetSystemMetrics(SM_CYSIZEFRAME);
	}
	else
	{
		sFrameSize.cx = GetSystemMetrics(SM_CXFIXEDFRAME);
		sFrameSize.cy = GetSystemMetrics(SM_CYFIXEDFRAME);
	}
	return sFrameSize;
}

LRESULT CALLBACK OptionProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG: // SetDlgCtrlID::SetDlgCtrlID
		{
			SetWindowPos(hDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOZORDER);
			InitData(hDlg);			
			if (hInst)
			{
				TCHAR siteName[1024];
				LoadString(hInst, IDS_OPEN_SITE, siteName, sizeof(siteName) - 1);
				hLihk.m_dwExtendedStyle |= HLINK_COMMANDBUTTON;
				hLihk.SetHyperLink(siteName);
				HWND hWND = GetDlgItem(hDlg, IDC_STATIC_LOGO_TEXT);
				hLihk.SubclassWindow(hWND);
			}

			// NO XP
			RECT wr;
			::GetWindowRect(hDlg, &wr);
			int Height = wr.right - wr.left;
			int Width = wr.bottom - wr.top;
			HRGN rgn = CreateRectRgn(0, 0, Height, Width);
			SetWindowRgn(hDlg, rgn, TRUE);
		}
		break;
	case WM_NCPAINT:
		{			
			DefWindowProc(hDlg, message, wParam, lParam);

			RECT wr;
			::GetWindowRect(hDlg, &wr);
			int Height = wr.right - wr.left;
			int Width = wr.bottom - wr.top;

			HDC hdc = GetWindowDC(hDlg);
			COLORREF color1 = RGB(255,255,255);
			COLORREF color2 = RGB(0,0,0);

			SIZE size = GetFrameSize(hDlg);
			RECT rect = {0, 0, Height, size.cy};  
			FillRect(hdc, &rect, CreateSolidBrush(color1));

			rect.top = Width - size.cy;
			rect.bottom = Width; 
			FillRect(hdc, &rect, CreateSolidBrush(color1));  

			rect.top = 0;
			rect.right = size.cx;  
			FillRect(hdc, &rect, CreateSolidBrush(color1));    

			rect.right = Height;
			rect.left = rect.right - size.cx;  
			FillRect(hdc, &rect, CreateSolidBrush(color1));

			RECT rect2 = {0, 0, Height, 1};
			FillRect(hdc, &rect2, CreateSolidBrush(color2));//GetSysColor

			rect2.top = Width - 1;
			rect2.bottom = Width;
			FillRect(hdc, &rect2, CreateSolidBrush(color2));

			rect2.top = 0;
			rect2.right = 1;
			FillRect(hdc, &rect2, CreateSolidBrush(color2));  

			rect2.right = Height;
			rect2.left = rect2.right - 1;
			FillRect(hdc, &rect2, CreateSolidBrush(color2));

			ReleaseDC(hDlg, hdc);
		}
		return TRUE;
	case WM_CTLCOLORDLG:	
		return (DWORD)::CreateSolidBrush(RGB(255, 255, 255));
	case WM_CTLCOLORSTATIC:
		SetBkMode((HDC) wParam, TRANSPARENT);
		return (LRESULT)(HBRUSH)GetStockObject(NULL_BRUSH);
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
		{
			if (LOWORD(wParam) == IDOK)
			{
				if (SaveData(hDlg))
				{
					EndDialog(hDlg, LOWORD(wParam));
				}
			}
			if (LOWORD(wParam) == IDCANCEL)
			{
				hLihk.UnsubclassWindow();
				EndDialog(hDlg, LOWORD(wParam));
			}
			break;
		}
//*
		if (HIWORD(wParam) == BN_CLICKED)
		{
			if (GetDlgItem(hDlg, IDC_STATIC_LOGO_TEXT) == (HWND)lParam)
			{
//				g_strLaunch = hLihk.m_lpstrHyperLink;
//				hLihk.UnsubclassWindow();
				DWORD_PTR dwRet = (DWORD_PTR)::ShellExecute(0, _T("open"), hLihk.m_lpstrHyperLink, 0, 0, SW_SHOWNORMAL);
//				EndDialog(hDlg, 0);
			}
		}
//*/
		switch(LOWORD(wParam))
		{
		case IDC_CHECK_DLG_RUN_SHOW:
			if (IsDlgButtonChecked(hDlg, IDC_CHECK_DLG_RUN_SHOW))
			{
				HWND hEdit = GetDlgItem(hDlg, IDC_EDIT_WILE_SECOND);
				EnableWindow(hEdit, TRUE);
			}
			else
			{
				HWND hEdit = GetDlgItem(hDlg, IDC_EDIT_WILE_SECOND);
				EnableWindow(hEdit, FALSE);
			}
			break;
		}
		break;		
	}
	return 0;
}

DWORD g_WhiteTimeCur = 0;
DWORD g_WhiteTimeMax = MIN_WHILE_VALUE;

void InitRunDlg(HWND hDlg)
{
	if (g_bIsSecuriRun)
		CheckDlgButton(hDlg, IDC_CHECK_IS_MODE, BST_CHECKED);

	GetKeyData(STR_DLGWHILESHOW, g_WhiteTimeMax);
	HWND hWndCheckWhile = GetDlgItem(hDlg, IDC_CHECK_WHITE);
	TCHAR szLoadString[MAX_LOADSTRING];
	LoadString(hInst, IDS_STRING_WHILE, szLoadString, MAX_LOADSTRING);
	CAtlString strText = _T("");
	strText.Format(szLoadString, g_WhiteTimeMax);
	if (g_WhiteTimeMax == 0) g_WhiteTimeMax = MIN_WHILE_VALUE;
	SetWindowText(hWndCheckWhile, strText);

	DWORD dwIsWhile = IS_WHILE;
	GetKeyData(STR_DLGISWHILE, dwIsWhile);
	if (dwIsWhile == IS_WHILE)
		CheckDlgButton(hDlg, IDC_CHECK_WHITE, BST_CHECKED);
	if (g_WhiteTimeMax == 0)
	{
		EndDialog(hDlg, IDOK);
	}
	else
	{
		HWND hWndProgress = GetDlgItem(hDlg, IDC_PROGRESS_WHILE);
		SendMessage((HWND)hWndProgress, (UINT)PBM_SETRANGE, 0, MAKELPARAM (0, g_WhiteTimeMax - 1));		
		if (dwIsWhile == IS_WHILE)
			SetTimer(hDlg, 0, 1000, NULL);
	}
}

void SaveRunDlgData(HWND hDlg)
{
	if (IsDlgButtonChecked(hDlg, IDC_CHECK_DLG_RUN_SHOW)) SetKeyData(STR_DLGRUNSHOW, SW_HIDE);
	if (IsDlgButtonChecked(hDlg, IDC_CHECK_WHITE)) 
		SetKeyData(STR_DLGISWHILE, IS_WHILE);
	else 
		SetKeyData(STR_DLGISWHILE, IS_NOT_WHILE);
}

void UpdateTime(HWND hDlg)
{
	g_WhiteTimeCur++;
	if (g_WhiteTimeCur == g_WhiteTimeMax)
	{
		SaveRunDlgData(hDlg);
		KillTimer(hDlg, 0);
		EndDialog(hDlg, IDOK);
	}
	HWND hWndCheckWhile = GetDlgItem(hDlg, IDC_CHECK_WHITE);
	TCHAR szLoadString[MAX_LOADSTRING];
	LoadString(hInst, IDS_STRING_WHILE, szLoadString, MAX_LOADSTRING);
	CAtlString strText = _T("");
	strText.Format(szLoadString, g_WhiteTimeMax - g_WhiteTimeCur);
	SetWindowText(hWndCheckWhile, strText);
	HWND hWndProgress = GetDlgItem(hDlg, IDC_PROGRESS_WHILE);
	SendMessage((HWND)hWndProgress, (UINT)PBM_SETPOS, (WPARAM)g_WhiteTimeCur, 0);
}
BOOL bClick;

LRESULT CALLBACK DlgRunProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{			
			InitRunDlg(hDlg);
			if (hInst)
			{
				TCHAR siteName[1024];
				LoadString(hInst, IDS_OPEN_SITE, siteName, sizeof(siteName) - 1);
				hLihk.m_dwExtendedStyle |= HLINK_COMMANDBUTTON;
				hLihk.SetHyperLink(siteName);
				hLihk.SubclassWindow(GetDlgItem(hDlg, IDC_STATIC_LOGO_TEXT));
			}
		}
		return TRUE;
	case WM_LBUTTONDOWN:
		SendMessage(hDlg, WM_SYSCOMMAND, 0x0001 | (WORD)SC_MOVE, 0);
		break;
	case WM_TIMER:
		UpdateTime(hDlg);
		break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
		{
			if (LOWORD(wParam) == IDOK)
			{
				SaveRunDlgData(hDlg);
			}
			hLihk.UnsubclassWindow();
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		if (HIWORD(wParam) == BN_CLICKED)
		{
			if (GetDlgItem(hDlg, IDC_STATIC_LOGO_TEXT) == (HWND)lParam)
			{
				SaveRunDlgData(hDlg);
				KillTimer(hDlg, 0);
				g_strLaunch = hLihk.m_lpstrHyperLink;
				hLihk.UnsubclassWindow();
				EndDialog(hDlg, 0);
			}
		}
		switch(LOWORD(wParam))
		{
		case IDC_CHECK_WHITE:
			if (IsDlgButtonChecked(hDlg, IDC_CHECK_WHITE))
			{
				SetTimer(hDlg, 0, 1000, NULL);
			}
			else
			{
				KillTimer(hDlg, 0);
			}
			break;
		case IDC_CHECK_IS_MODE:
			if (IsDlgButtonChecked(hDlg, IDC_CHECK_IS_MODE))
			{
				g_bIsSecuriRun = TRUE;
			}
			else
			{
				g_bIsSecuriRun = FALSE;
			}
		}
		break;
	}
	return FALSE;
}

#include "ADD\ProgressByTime.h"
#include "ADD\BMPButton.h"

CProgressByTime g_Progress;
CBMPButton g_BMPButtonOK;
CBMPButton g_BMPButtonNOSafe;
SIZE BitmapSize;
HDC g_hDC = NULL;
HBITMAP g_Old_BMP = NULL;
int g_NumSplash = 0;

void DrawHLink(HWND hDlg, HDC hDC, UINT ID)
{
	HDC hDCW = GetDC(hDlg);
	HDC hDCH = CreateCompatibleDC(hDCW);

	HWND hWnd = GetDlgItem(hDlg, ID);
	RECT rect = {0};
	RECT rectW = {0};
	GetWindowRect(hWnd, &rect);
	GetWindowRect(hDlg, &rectW);

	SIZE sHLink = {rect.right - rect.left, rect.bottom - rect.top};

	HBITMAP hBMP = CreateCompatibleBitmap(hDC, sHLink.cx, sHLink.cy);
	HBITMAP Old_BMP = (HBITMAP)SelectObject(hDCH, (HGDIOBJ)hBMP);

	BitBlt(hDCH, 0, 0, sHLink.cx, sHLink.cy, g_hDC, rect.left - rectW.left, rect.top - rectW.top, SRCCOPY);
	hLihk.DoPaint(hDCH);
	BitBlt(hDC, rect.left - rectW.left, rect.top - rectW.top, sHLink.cx, sHLink.cy, hDCH, 0, 0, SRCCOPY);	

	SelectObject(hDCH, (HGDIOBJ)hBMP);
	DeleteObject(hBMP);
	DeleteDC(hDCH);

	ReleaseDC(hDlg, hDCW);
}

LRESULT CALLBACK DlgRunProcNew(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{	
	switch (message)
	{
	case WM_INITDIALOG:
		{		
			InitRunDlg(hDlg);
			if (hInst)
			{
				TCHAR siteName[1024];
				LoadString(hInst, IDS_OPEN_SITE, siteName, sizeof(siteName) - 1);
				hLihk.m_dwExtendedStyle |= HLINK_COMMANDBUTTON;
				hLihk.SetHyperLink(siteName);
				hLihk.SubclassWindow(GetDlgItem(hDlg, IDC_STATIC_LOGO_TEXT));
			}

			TCHAR Caption[MAX_LOADSTRING];
			LoadString(hInst, IDS_SPLASH_CAPTION, Caption, MAX_LOADSTRING);
			SetWindowText(hDlg, Caption);

			HBITMAP hBitmap = ::LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BITMAP_SPLASH_INACTIVE));
			BITMAP bm = {NULL};
			GetObject(hBitmap, sizeof(BITMAP), (LPSTR)&bm);
			BitmapSize.cx = bm.bmWidth;
			BitmapSize.cy = bm.bmHeight;

			HDC hDC = GetDC(hDlg);

			g_hDC = CreateCompatibleDC(hDC);
			HBITMAP hBMP = CreateCompatibleBitmap(hDC, BitmapSize.cx, BitmapSize.cy);
			g_Old_BMP = (HBITMAP)SelectObject(g_hDC, (HGDIOBJ)hBMP);
						
			m_ImageList = ImageList_Create(BitmapSize.cx, BitmapSize.cy, ILC_MASK | ILC_COLORDDB, 3, 1);
			ImageList_AddMasked(m_ImageList, hBitmap, RGB(0,1,0));
			hBitmap = ::LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BITMAP_SPLASH_GREEN));
			ImageList_AddMasked(m_ImageList, hBitmap, RGB(0,1,0));
			hBitmap = ::LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BITMAP_SPLASH_RED));
			ImageList_AddMasked(m_ImageList, hBitmap, RGB(0,1,0));

			ImageList_DrawEx(m_ImageList, 0, g_hDC, 0, 0, BitmapSize.cx, BitmapSize.cy, CLR_NONE, CLR_NONE, ILD_TRANSPARENT);

			ReleaseDC(hDlg, hDC);

			DWORD WhiteTime = 0;
			GetKeyData(STR_DLGWHILESHOW, WhiteTime);

			POINT point = {17, 296};
			BOOL bRez = g_Progress.Init(hDlg,
				point,
				::LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BITMAP_PROGRESS)),
				0,
				SECONDS(WhiteTime),
				RGB(255, 255, 255));
			assert(bRez);

			POINT pointOK = {18, 32};
			RECT rectOK = {0, 0, 104, 104};
			g_BMPButtonOK.Init(hDlg, 
				IDOK,
				pointOK,
				rectOK,
				_ELIPTIC);

			POINT pointBNOS = {18, 148};
			RECT rectBNOS = {0, 0, 104, 104};
			g_BMPButtonNOSafe.Init(hDlg, 
				IDC_BUTTON_NO_SAFE,
				pointBNOS,
				rectBNOS,
				_ELIPTIC);
		}
		return TRUE;
	case WM_TIMER:
		g_Progress.OnTime(wParam);
		break;
	case WM_PAINT: 
		{
			HDC hDC = GetDC(hDlg);

			ImageList_DrawEx(m_ImageList, g_NumSplash, g_hDC, 0, 0, BitmapSize.cx, BitmapSize.cy, CLR_NONE, CLR_NONE, ILD_TRANSPARENT);

			g_BMPButtonNOSafe.OnDraw(g_hDC);
			g_BMPButtonOK.OnDraw(g_hDC);
			g_Progress.OnDraw(g_hDC);

			DrawHLink(hDlg, g_hDC, IDC_STATIC_LOGO_TEXT);

			BitBlt(hDC, 0, 0, BitmapSize.cx, BitmapSize.cy, g_hDC, 0, 0, SRCCOPY);
			ReleaseDC(hDlg, hDC);
		}
		break;
	case WM_MOUSEMOVE:
		{
			g_NumSplash = 0;
			if (g_BMPButtonOK.OnMouseMove(lParam))
			{
				g_NumSplash = 1;
				g_Progress.Pause();
				SetCapture(hDlg);
			}
			else
			{
				if (g_BMPButtonNOSafe.OnMouseMove(lParam))
				{
					g_NumSplash = 2;
					g_Progress.Pause();
					SetCapture(hDlg);
				}
				else
				{
					g_Progress.Pause(FALSE);
					ReleaseCapture();
				}
			}
		}
		break;
	case WM_LBUTTONDOWN:
		if (!g_BMPButtonNOSafe.OnLButtonDown(lParam) &&
			!g_BMPButtonOK.OnLButtonDown(lParam))
			SendMessage(hDlg, WM_SYSCOMMAND, 0x0001 | (WORD)SC_MOVE, 0);
		break;
	case WM_LBUTTONUP:
		g_BMPButtonNOSafe.OnLButtonUp(lParam);
		g_BMPButtonOK.OnLButtonUp(lParam);
		break;
	case WM_CTLCOLORSTATIC:
		{
			if ((HWND)lParam == GetDlgItem(hDlg, IDC_STATIC_LOGO_TEXT))
			{
				SetBkMode((HDC) wParam, TRANSPARENT);
				return (LRESULT)(HBRUSH)GetStockObject(NULL_BRUSH);
			}
		}
		break;
	case WM_PROGRESS_END:
		EndDialog(hDlg, IDOK);
		break;
	case WM_ERASEBKGND:
		{
			HDC hDC = GetDC(hDlg);
			RECT rect = {0};
			GetClientRect(hDlg, &rect);

			FillRect(g_hDC, &rect, CreateSolidBrush(RGB(0, 0, 0)));
			ImageList_DrawEx(m_ImageList, g_NumSplash, g_hDC, 0, 0, BitmapSize.cx, BitmapSize.cy, CLR_NONE, CLR_NONE, ILD_TRANSPARENT);

			g_BMPButtonNOSafe.OnDraw(g_hDC);
			g_BMPButtonOK.OnDraw(g_hDC);
			g_Progress.OnDraw(g_hDC);

			DrawHLink(hDlg, g_hDC, IDC_STATIC_LOGO_TEXT);

			BitBlt(hDC, 0, 0, BitmapSize.cx, BitmapSize.cy, g_hDC, 0, 0, SRCCOPY);
			ReleaseDC(hDlg, hDC);			

			return TRUE;
		}
		break;
	case WM_DESTROY:
		{
			HBITMAP hBMP = (HBITMAP)SelectObject(g_hDC, (HGDIOBJ)g_Old_BMP);
			DeleteObject(hBMP);
			DeleteDC(g_hDC);
			ImageList_Destroy(m_ImageList);
		}
		break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
		{
			if (LOWORD(wParam) == IDOK)
			{
				SaveRunDlgData(hDlg);
			}
			if ((LOWORD(wParam) == IDCANCEL) && (g_bEmbedding == TRUE))
			{
				return FALSE;
			}			
			hLihk.UnsubclassWindow();
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		if (HIWORD(wParam) == BN_CLICKED)
		{
			if (GetDlgItem(hDlg, IDC_STATIC_LOGO_TEXT) == (HWND)lParam)
			{
				SaveRunDlgData(hDlg);
				KillTimer(hDlg, 0);
				g_strLaunch = hLihk.m_lpstrHyperLink;
				hLihk.UnsubclassWindow();
				EndDialog(hDlg, 0);
			}
		}
		if (LOWORD(wParam) == IDC_BUTTON_NO_SAFE)
		{
			g_bIsSecuriRun = FALSE;
			SaveRunDlgData(hDlg);
			hLihk.UnsubclassWindow();
			EndDialog(hDlg, IDOK);
			return TRUE;
		}
		break;
	}

	return FALSE;
}

void InitDlgSetAsDefault(HWND hDlg)
{
	CheckDlgButton(hDlg, IDC_CHECK_IS_DEF, BST_CHECKED);
}

LRESULT CALLBACK DlgSetAsDefaultProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		InitDlgSetAsDefault(hDlg);
		return TRUE;
	case WM_TIMER:
		UpdateTime(hDlg);
		break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
		{
			if (IsDlgButtonChecked(hDlg, IDC_CHECK_IS_DEF))
				SetKeyData(STR_CHECKISDEF, (DWORD)BST_CHECKED);
			else
				SetKeyData(STR_CHECKISDEF, (DWORD)BST_UNCHECKED);
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}