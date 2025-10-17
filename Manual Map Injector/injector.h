#pragma once

#include <Windows.h>
#include <iostream>
#include <fstream>
#include <TlHelp32.h>
#include <stdio.h>
#include <string>

typedef HINSTANCE(WINAPI* f_LL)(const char* lpLibFilename);
typedef FARPROC(WINAPI* f_GPA)(HMODULE hModule, LPCSTR lpProcName);
typedef BOOL(WINAPI* f_DEP)(void* hDll, DWORD dwReason, void* pReserved);

#ifdef _WIN64
typedef BOOL(WINAPIV* f_RAFT)(PRUNTIME_FUNCTION FunctionTable, DWORD EntryCount, DWORD64 BaseAddress);
#endif

struct MM_DATA
{
	f_LL pLL;
	f_GPA pGPA;
#ifdef _WIN64
	f_RAFT pRAFT;
#endif
	BYTE* pbase;
	HINSTANCE hMod;
	DWORD fdwReasonParam;
	LPVOID reservedParam;
	BOOL SEHSupport;
};

bool MM_DLL(HANDLE hProc, BYTE* pSrcData, SIZE_T FileSize, bool ClearHeader = true, bool ClearNonNeededSections = true, bool AdjustProtections = true, bool SEHExceptionSupport = true, DWORD fdwReason = DLL_PROCESS_ATTACH, LPVOID lpReserved = 0);

void __stdcall SC(MM_DATA* pData);
