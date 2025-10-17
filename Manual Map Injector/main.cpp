#include "injector.h"
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>

using namespace std;

bool IsCorrectTargetArchitecture(HANDLE hProc) {
    BOOL bTarget = FALSE;
    if (!IsWow64Process(hProc, &bTarget)) {
        wprintf(L"Can't confirm target process architecture: 0x%X\n", GetLastError());
        return false;
    }

    BOOL bHost = FALSE;
    IsWow64Process(GetCurrentProcess(), &bHost);

    return (bTarget == bHost);
}

DWORD GetProcessIdByName(wchar_t* name) {
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (snapshot == INVALID_HANDLE_VALUE) {
        wprintf(L"CreateToolhelp32Snapshot failed: 0x%X\n", GetLastError());
        return 0;
    }

    if (Process32First(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, name) == 0) {
                CloseHandle(snapshot);
                return entry.th32ProcessID;
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return 0;
}

void SetConsoleColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void TypeEffect(const wstring& message, int delay_ms) {
    for (wchar_t c : message) {
        wcout << c << flush;
        this_thread::sleep_for(chrono::milliseconds(delay_ms));
    }
}

int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) {
    SetConsoleColor(10);
    TypeEffect(L"[+] Welcome to H-zz-H Injector!\n", 20);

    if (argc != 3) {
        wcout << L"Usage: injector.exe your.dll game.exe\n";
        system("pause");
        return 0;
    }

    wchar_t* dllPath = argv[1];
    wchar_t* processName = argv[2];
    DWORD PID = GetProcessIdByName(processName);

    if (PID == 0) {
        wcout << L"Process not found: " << processName << L"\n";
        system("pause");
        return -1;
    }

    wcout << L"Injecting into: " << PID << L"\n";

    TOKEN_PRIVILEGES priv = { 0 };
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        priv.PrivilegeCount = 1;
        priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &priv.Privileges[0].Luid))
            AdjustTokenPrivileges(hToken, FALSE, &priv, 0, NULL, NULL);

        CloseHandle(hToken);
    }

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, PID);
    if (!hProc) {
        DWORD Err = GetLastError();
        wcout << L"OpenProcess failed: 0x" << hex << Err << L"\n";
        system("pause");
        return -2;
    }

    if (!IsCorrectTargetArchitecture(hProc)) {
        wcout << L"Invalid Process Architecture.\n";
        CloseHandle(hProc);
        system("pause");
        return -3;
    }

    if (GetFileAttributes(dllPath) == INVALID_FILE_ATTRIBUTES) {
        wcout << L"Dll file doesn't exist: " << dllPath << L"\n";
        CloseHandle(hProc);
        system("pause");
        return -4;
    }

    ifstream File(dllPath, ios::binary | ios::ate);
    if (File.fail()) {
        wcout << L"Opening the file failed: " << hex << File.rdstate() << L"\n";
        File.close();
        CloseHandle(hProc);
        system("pause");
        return -5;
    }

    auto FileSize = File.tellg();
    if (FileSize < 0x1000) {
        wcout << L"Filesize invalid.\n";
        File.close();
        CloseHandle(hProc);
        system("pause");
        return -6;
    }

    BYTE* pSrcData = new BYTE[(UINT_PTR)FileSize];
    if (!pSrcData) {
        wcout << L"Memory allocation failed.\n";
        File.close();
        CloseHandle(hProc);
        system("pause");
        return -7;
    }

    File.seekg(0, ios::beg);
    File.read(reinterpret_cast<char*>(pSrcData), FileSize);
    File.close();

    wcout << L"Injecting...\n";
    if (!ManualMapDll(hProc, pSrcData, FileSize)) {
        delete[] pSrcData;
        CloseHandle(hProc);
        wcout << L"Error while injecting.\n";
        system("pause");
        return -8;
    }
    delete[] pSrcData;

    CloseHandle(hProc);
    wcout << L"Injection successful! Made by H-zz-H (updated by brainrot02)\n";
    system("pause");
    return 0;
}
