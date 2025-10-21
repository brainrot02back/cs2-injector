#include "injector.h"
#include <stdio.h>
#include <string>
#include <iostream>
#include <windows.h>
#include <thread>
#include <chrono>
#include <string>

#if defined(DISABLE_OUTPUT)
#define ILog(data, ...)
#else
#define ILog(text, ...) printf(text, __VA_ARGS__);
#endif

#ifdef _WIN64
#define CURRENT_ARCH IMAGE_FILE_MACHINE_AMD64
#else
#define CURRENT_ARCH IMAGE_FILE_MACHINE_I386
#endif

void setColor(int color) {
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void typeEffect(const std::string& message, int delay_ms) {
	for (char c : message) {
		std::cout << c << std::flush;
		std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
	}
}

bool ManualMapDll(HANDLE hProc, BYTE* pSrcData, SIZE_T FileSize, bool ClearHeader, bool ClearNonNeededSections, bool AdjustProtections, bool SEHExceptionSupport, DWORD fdwReason, LPVOID lpReserved) {
	IMAGE_NT_HEADERS* pOldNtHeader = nullptr;
	IMAGE_OPTIONAL_HEADER* pOldOptHeader = nullptr;
	IMAGE_FILE_HEADER* pOldFileHeader = nullptr;
	BYTE* pTargetBase = nullptr;
	system("chcp 65001 >NUL");

	setColor(10);
	typeEffect("[+] Welcome to H-zz-H Injector!\n", 20);

	setColor(10);
	if (reinterpret_cast<IMAGE_DOS_HEADER*>(pSrcData)->e_magic != 0x5A4D) { //"MZ"
		setColor(4);
		ILog("Invalid file\n");
		return false;
	}

	pOldNtHeader = reinterpret_cast<IMAGE_NT_HEADERS*>(pSrcData + reinterpret_cast<IMAGE_DOS_HEADER*>(pSrcData)->e_lfanew);
	pOldOptHeader = &pOldNtHeader->OptionalHeader;
	pOldFileHeader = &pOldNtHeader->FileHeader;

	if (pOldFileHeader->Machine != CURRENT_ARCH) {
		setColor(4);
		ILog("Invalid platform\n");
		return false;
	}

	ILog("File ok\n");

	pTargetBase = reinterpret_cast<BYTE*>(VirtualAllocEx(hProc, nullptr, pOldOptHeader->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	if (!pTargetBase) {
		setColor(4);
		ILog("Target process memory allocation failed (ex) 0x%X\n", GetLastError());
		return false;
	}

	DWORD oldp = 0;
	VirtualProtectEx(hProc, pTargetBase, pOldOptHeader->SizeOfImage, PAGE_EXECUTE_READWRITE, &oldp);

	MANUAL_MAPPING_DATA data{ 0 };
	data.pLoadLibraryA = LoadLibraryA;
	data.pGetProcAddress = GetProcAddress;
#ifdef _WIN64
	data.pRtlAddFunctionTable = (f_RtlAddFunctionTable)RtlAddFunctionTable;
#else
	SEHExceptionSupport = false;
#endif
	data.pbase = pTargetBase;
	data.fdwReasonParam = fdwReason;
	data.reservedParam = lpReserved;
	data.SEHSupport = SEHExceptionSupport;

	if (!WriteProcessMemory(hProc, pTargetBase, pSrcData, 0x1000, nullptr)) {
		setColor(4);
		ILog("Can't write file header 0x%X\n", GetLastError());
		VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
		return false;
	}

	IMAGE_SECTION_HEADER* pSectionHeader = IMAGE_FIRST_SECTION(pOldNtHeader);
	for (UINT i = 0; i != pOldFileHeader->NumberOfSections; ++i, ++pSectionHeader) {
		if (pSectionHeader->SizeOfRawData) {
			if (!WriteProcessMemory(hProc, pTargetBase + pSectionHeader->VirtualAddress, pSrcData + pSectionHeader->PointerToRawData, pSectionHeader->SizeOfRawData, nullptr)) {
				setColor(4);
				ILog("Can't map sections: 0x%x\n", GetLastError());
				VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
				return false;
			}
		}
	}

	BYTE* MappingDataAlloc = reinterpret_cast<BYTE*>(VirtualAllocEx(hProc, nullptr, sizeof(MANUAL_MAPPING_DATA), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	if (!MappingDataAlloc) {
		setColor(4);
		ILog("Target process mapping allocation failed (ex) 0x%X\n", GetLastError());
		VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
		return false;
	}

	if (!WriteProcessMemory(hProc, MappingDataAlloc, &data, sizeof(MANUAL_MAPPING_DATA), nullptr)) {
		setColor(4);
		ILog("Can't write mapping 0x%X\n", GetLastError());
		VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
		VirtualFreeEx(hProc, MappingDataAlloc, 0, MEM_RELEASE);
		return false;
	}

	void* pShellcode = VirtualAllocEx(hProc, nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!pShellcode) {
		setColor(4);
		ILog("Memory shellcode allocation failed (ex) 0x%X\n", GetLastError());
		VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
		VirtualFreeEx(hProc, MappingDataAlloc, 0, MEM_RELEASE);
		return false;
	}

	if (!WriteProcessMemory(hProc, pShellcode, Shellcode, 0x1000, nullptr)) {
		setColor(4);
		ILog("Can't write shellcode 0x%X\n", GetLastError());
		VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
		VirtualFreeEx(hProc, MappingDataAlloc, 0, MEM_RELEASE);
		VirtualFreeEx(hProc, pShellcode, 0, MEM_RELEASE);
		return false;
	}

	ILog("Made\n", pTargetBase);
	ILog("by\n", MappingDataAlloc);
	ILog("H-zz-H\n", pShellcode);

	ILog("https://discord.gg/HNYaKzKZQU\n");

#ifdef _DEBUG
	ILog("https://discord.gg/HNYaKzKZQU\n", Shellcode);
	ILog("Finished Injecting...\n", pShellcode);
	system("pause");
#endif

	HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(pShellcode), MappingDataAlloc, 0, nullptr);
	if (!hThread) {
		setColor(4);
		ILog("Thread creation failed 0x%X\n", GetLastError());
		VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
		VirtualFreeEx(hProc, MappingDataAlloc, 0, MEM_RELEASE);
		VirtualFreeEx(hProc, pShellcode, 0, MEM_RELEASE);
		return false;
	}
	CloseHandle(hThread);

	ILog("[+] Successfully injected into your Process\n", pShellcode);

	HINSTANCE hCheck = NULL;
	while (!hCheck) {
		DWORD exitcode = 0;
		GetExitCodeProcess(hProc, &exitcode);
		if (exitcode != STILL_ACTIVE) {
			setColor(4);
			ILog("Process crashed, exit code: %d\n", exitcode);
			return false;
		}

		MANUAL_MAPPING_DATA data_checked{ 0 };
		ReadProcessMemory(hProc, MappingDataAlloc, &data_checked, sizeof(data_checked), nullptr);
		hCheck = data_checked.hMod;

		if (hCheck == (HINSTANCE)0x404040) {
			setColor(4);
			ILog("Wrong mapping ptr\n");
			VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
			VirtualFreeEx(hProc, MappingDataAlloc, 0, MEM_RELEASE);
			VirtualFreeEx(hProc, pShellcode, 0, MEM_RELEASE);
			return false;
		}
		else if (hCheck == (HINSTANCE)0x505050) {
			setColor(4);
			ILog("WARNING: Exception support failed!\n");
		}

		Sleep(10);
	}

	BYTE* emptyBuffer = (BYTE*)malloc(1024 * 1024 * 20);
	if (emptyBuffer == nullptr) {
		setColor(4);
		ILog("Unable to allocate memory\n");
		return false;
	}
	memset(emptyBuffer, 0, 1024 * 1024 * 20);

	if (ClearHeader) {
		if (!WriteProcessMemory(hProc, pTargetBase, emptyBuffer, 0x1000, nullptr)) {
			setColor(4);
			ILog("WARNING!: Can't clear HEADER\n");
		}
	}

	if (ClearNonNeededSections) {
		pSectionHeader = IMAGE_FIRST_SECTION(pOldNtHeader);
		for (UINT i = 0; i != pOldFileHeader->NumberOfSections; ++i, ++pSectionHeader) {
			if (pSectionHeader->Misc.VirtualSize) {
				if ((SEHExceptionSupport ? 0 : strcmp((char*)pSectionHeader->Name, ".pdata") == 0) ||
					strcmp((char*)pSectionHeader->Name, ".rsrc") == 0 ||
					strcmp((char*)pSectionHeader->Name, ".reloc") == 0) {
					setColor(4);
					ILog("Processing %s removal\n", pSectionHeader->Name);
					if (!WriteProcessMemory(hProc, pTargetBase + pSectionHeader->VirtualAddress, emptyBuffer, pSectionHeader->Misc.VirtualSize, nullptr)) {
						ILog("Can't clear section %s: 0x%x\n", pSectionHeader->Name, GetLastError());
					}
				}
			}
		}
	}

	if (AdjustProtections) {
		pSectionHeader = IMAGE_FIRST_SECTION(pOldNtHeader);
		for (UINT i = 0; i != pOldFileHeader->NumberOfSections; ++i, ++pSectionHeader) {
			if (pSectionHeader->Misc.VirtualSize) {
				DWORD old = 0;
				DWORD newP = PAGE_READONLY;

				if ((pSectionHeader->Characteristics & IMAGE_SCN_MEM_WRITE) > 0) {
					newP = PAGE_READWRITE;
				}
				else if ((pSectionHeader->Characteristics & IMAGE_SCN_MEM_EXECUTE) > 0) {
					newP = PAGE_EXECUTE_READ;
				}
				if (VirtualProtectEx(hProc, pTargetBase + pSectionHeader->VirtualAddress, pSectionHeader->Misc.VirtualSize, newP, &old)) {
					setColor(4);
					ILog("section %s set as %lX\n", (char*)pSectionHeader->Name, newP);
				}
				else {
					setColor(4);
					ILog("FAIL: section %s not set as %lX\n", (char*)pSectionHeader->Name, newP);
				}
			}
		}
		DWORD old = 0;
		VirtualProtectEx(hProc, pTargetBase, IMAGE_FIRST_SECTION(pOldNtHeader)->VirtualAddress, PAGE_READONLY, &old);
	}

	if (!WriteProcessMemory(hProc, pShellcode, emptyBuffer, 0x1000, nullptr)) {
		setColor(14);
		ILog("WARNING: Can't clear shellcode\n");
	}
	if (!VirtualFreeEx(hProc, pShellcode, 0, MEM_RELEASE)) {
		setColor(14);
		ILog("WARNING: can't release shell code memory\n");
	}
	if (!VirtualFreeEx(hProc, MappingDataAlloc, 0, MEM_RELEASE)) {
		setColor(14);
		ILog("WARNING: can't release mapping data memory\n");
	}

	free(emptyBuffer);
	return true;
}

#define RELOC_FLAG32(RelInfo) ((RelInfo >> 0x0C) == IMAGE_REL_BASED_HIGHLOW)
#define RELOC_FLAG64(RelInfo) ((RelInfo >> 0x0C) == IMAGE_REL_BASED_DIR64)

#ifdef _WIN64
#define RELOC_FLAG RELOC_FLAG64
#else
#define RELOC_FLAG RELOC_FLAG32
#endif

#pragma runtime_checks( "", off )
#pragma optimize( "", off )
void __stdcall Shellcode(MANUAL_MAPPING_DATA* pData) {
	if (!pData) {
		pData->hMod = (HINSTANCE)0x404040;
		return;
	}

	BYTE* pBase = pData->pbase;
	auto* pOpt = &reinterpret_cast<IMAGE_NT_HEADERS*>(pBase + reinterpret_cast<IMAGE_DOS_HEADER*>((uintptr_t)pBase)->e_lfanew)->OptionalHeader;

	auto _LoadLibraryA = pData->pLoadLibraryA;
	auto _GetProcAddress = pData->pGetProcAddress;
#ifdef _WIN64
	auto _RtlAddFunctionTable = pData->pRtlAddFunctionTable;
#endif
	auto _DllMain = reinterpret_cast<f_DLL_ENTRY_POINT>(pBase + pOpt->AddressOfEntryPoint);

	BYTE* LocationDelta = pBase - pOpt->ImageBase;
	if (LocationDelta) {
		if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size) {
			auto* pRelocData = reinterpret_cast<IMAGE_BASE_RELOCATION*>(pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
			const auto* pRelocEnd = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<uintptr_t>(pRelocData) + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size);
			while (pRelocData < pRelocEnd && pRelocData->SizeOfBlock) {
				UINT AmountOfEntries = (pRelocData->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
				WORD* pRelativeInfo = reinterpret_cast<WORD*>(pRelocData + 1);

				for (UINT i = 0; i != AmountOfEntries; ++i, ++pRelativeInfo) {
					if (RELOC_FLAG(*pRelativeInfo)) {
						UINT_PTR* pPatch = reinterpret_cast<UINT_PTR*>(pBase + pRelocData->VirtualAddress + ((*pRelativeInfo) & 0xFFF));
						*pPatch += reinterpret_cast<UINT_PTR>(LocationDelta);
					}
				}
				pRelocData = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<BYTE*>(pRelocData) + pRelocData->SizeOfBlock);
			}
		}
	}

	if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size) {
		auto* pImportDescr = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
		while (pImportDescr->Name) {
			char* szMod = reinterpret_cast<char*>(pBase + pImportDescr->Name);
			HINSTANCE hDll = _LoadLibraryA(szMod);

			ULONG_PTR* pThunkRef = reinterpret_cast<ULONG_PTR*>(pBase + pImportDescr->OriginalFirstThunk);
			ULONG_PTR* pFuncRef = reinterpret_cast<ULONG_PTR*>(pBase + pImportDescr->FirstThunk);

			if (!pThunkRef)
				pThunkRef = pFuncRef;

			for (; *pThunkRef; ++pThunkRef, ++pFuncRef) {
				if (IMAGE_SNAP_BY_ORDINAL(*pThunkRef)) {
					*pFuncRef = (ULONG_PTR)_GetProcAddress(hDll, reinterpret_cast<char*>(*pThunkRef & 0xFFFF));
				}
				else {
					auto* pImport = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(pBase + (*pThunkRef));
					*pFuncRef = (ULONG_PTR)_GetProcAddress(hDll, pImport->Name);
				}
			}
			++pImportDescr;
		}
	}

	if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size) {
		auto* pTLS = reinterpret_cast<IMAGE_TLS_DIRECTORY*>(pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);
		auto* pCallback = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(pTLS->AddressOfCallBacks);
		for (; pCallback && *pCallback; ++pCallback)
			(*pCallback)(pBase, DLL_PROCESS_ATTACH, nullptr);
	}

	bool ExceptionSupportFailed = false;

#ifdef _WIN64

	if (pData->SEHSupport) {
		auto excep = pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
		if (excep.Size) {
			if (!_RtlAddFunctionTable(
				reinterpret_cast<IMAGE_RUNTIME_FUNCTION_ENTRY*>(pBase + excep.VirtualAddress),
				excep.Size / sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY), (DWORD64)pBase)) {
				ExceptionSupportFailed = true;
			}
		}
	}

#endif

	_DllMain(pBase, pData->fdwReasonParam, pData->reservedParam);

	if (ExceptionSupportFailed)
		pData->hMod = reinterpret_cast<HINSTANCE>(0x505050);
	else
		pData->hMod = reinterpret_cast<HINSTANCE>(pBase);
}
