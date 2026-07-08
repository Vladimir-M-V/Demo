#include "ProcessList.h"
#include <psapi.h>
#include <stdio.h>
#include <tchar.h>

#define DEF_MAX_PROCCESS 512

CProcessList::CProcessList(void)
{
	m_bLogProcess = false;
	DWORD nProccessCount = DEF_MAX_PROCCESS;
	DWORD nProcessNeeded = 0;
	
	m_ListHendlers = NULL;
	m_nProccessCount = 0;

	m_ListHendlers = (DWORD*)malloc(nProccessCount * sizeof(DWORD));
	if (m_ListHendlers != NULL)
	{
		do
		{
			if (EnumProcesses(m_ListHendlers, nProccessCount * sizeof(DWORD), &nProcessNeeded))
			{
				m_nProccessCount = nProcessNeeded / sizeof(DWORD);
				if (nProccessCount == (nProcessNeeded / sizeof(DWORD)))
				{
					free(m_ListHendlers);
					m_ListHendlers = NULL;

					nProccessCount += DEF_MAX_PROCCESS;
					m_ListHendlers = (DWORD*)malloc(nProccessCount * sizeof(DWORD));
					continue;
				}
			}
			else
			{
				free(m_ListHendlers);
				m_ListHendlers = NULL;
				m_nProccessCount = 0;
			}
			break;
		} while (1);
	}
}

CProcessList::~CProcessList(void)
{
	if (m_ListHendlers != NULL)
	{
		free(m_ListHendlers);
		m_ListHendlers = NULL;
		m_nProccessCount = 0;
	}
}

void CProcessList::EnableLog(void)
{
	m_bLogProcess = true;
	Log();
}

BOOL CProcessList::KillProcessByName(std::string strFilter, DWORD pTime)
{
	BOOL bResult = false;
	DWORD i;
	TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");

	strFilter.erase(std::remove(strFilter.begin(), strFilter.end(), '*'), strFilter.end());

	for (i = 0; i < m_nProccessCount; i++)
	{
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, m_ListHendlers[i]);

		if (NULL != hProcess)
		{
			HMODULE hMod;
			DWORD cbNeeded;
			std::basic_string<TCHAR> converted(strFilter.begin(), strFilter.end());
			const TCHAR* tchar = converted.c_str();

			if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
			{
				GetModuleBaseName(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(TCHAR));
			}

			if (m_bLogProcess)
				_tprintf(TEXT("Name1:%s Name2:%s\n"), szProcessName, tchar);
			if (_tcsstr(szProcessName, tchar) != NULL)
			{
				KillProcessByHandle(&hProcess, pTime, m_ListHendlers[i]);
			}
		}

		if (hProcess != NULL)
			CloseHandle(hProcess);
	}

	return bResult;
}

BOOL CProcessList::KillProcessByHandle(HANDLE *phProcess, DWORD pTime, DWORD pPid)
{
	BOOL tpBool = false;

	if (CheckTime(*phProcess, pTime))
	{
		CloseHandle(*phProcess);
		*phProcess = NULL;
		HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pPid);
		tpBool = TerminateProcess(hProcess, 2);
		if (hProcess != NULL)
			CloseHandle(hProcess);
	}

	return tpBool;
}

__int64 to_int64(FILETIME ft)
{
	return static_cast<__int64>(ft.dwHighDateTime) << 32 | ft.dwLowDateTime;
}

BOOL CProcessList::CheckTime(HANDLE phProcess, DWORD pTime)
{
	FILETIME createTime;
	FILETIME exitTime;
	FILETIME kernelTime;
	FILETIME userTime;
	FILETIME currentTime;
	SYSTEMTIME localTime;
	FILETIME lt;
	__int64 timeCurrent;
	__int64 timeProcess;
	if (GetProcessTimes(phProcess, &createTime, &exitTime, &kernelTime, &userTime) != -1)
	{
		GetSystemTime(&localTime);
		SystemTimeToFileTime(&localTime, &lt);

		timeCurrent = to_int64(lt);
		timeProcess = to_int64(createTime);

		if (timeCurrent >= timeProcess + (__int64)((pTime * 1000 * 1000 * 1000) * 60))
		{
			return true;
		}
	}

	return false;
}

void aux_PrintProcessNameAndID(DWORD i, DWORD processID)
{
	TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");

	// Get a handle to the process.

	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

	// Get the process name.

	if (NULL != hProcess)
	{
		HMODULE hMod;
		DWORD cbNeeded;

		if (EnumProcessModules(hProcess, &hMod, sizeof(hMod),
			&cbNeeded))
		{
			GetModuleBaseName(hProcess, hMod, szProcessName,
				sizeof(szProcessName) / sizeof(TCHAR));
		}
	}

	// Print the process name and identifier.

	_tprintf(TEXT("Index:%u Name:%s  (PID: %u)\n"), i, szProcessName, processID);

	// Release the handle to the process.

	if (hProcess != NULL)
		CloseHandle(hProcess);
}

void CProcessList::Log(void)
{
	DWORD i;
	for (i = 0; i < m_nProccessCount; i++)
	{
		if (m_ListHendlers[i] != 0)
		{
			aux_PrintProcessNameAndID(i, m_ListHendlers[i]);
		}
	}
}
