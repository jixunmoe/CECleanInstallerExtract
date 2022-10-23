// ExtractorDLL.cpp : Defines the exported functions for the DLL.
//

#include "pch.h"
#include "framework.h"
#include "ExtractorDLL.h"

#include <string>
#include <vector>
#include <utility>

typedef LPBYTE(__stdcall* virtual_alloc_fn)(
	LPBYTE lpAddress,
	SIZE_T dwSize,
	DWORD flAllocationType,
	DWORD flProtect
	);

virtual_alloc_fn real_virtual_alloc = nullptr;
DWORD _callback_pid = 0;
LPBYTE _callback_address = nullptr;

std::vector<std::pair<LPBYTE, SIZE_T>> old_allocs;

BYTE ceDownloadUrlPrefix[16] = {
	0x68, 0x00, 0x74, 0x00, 0x74, 0x00, 0x70, 0x00, 0x73, 0x00, 0x3A, 0x00, 0x2F, 0x00, 0x2F, 0x00
};

int alloc_count = 3;

void do_search(LPBYTE addr, SIZE_T size) {
	LPBYTE end_address = addr + size - sizeof(ceDownloadUrlPrefix);
	for (LPBYTE p = addr; p < end_address; p++) {
		try {
			if (memcmp(p, ceDownloadUrlPrefix, sizeof(ceDownloadUrlPrefix)) != 0) {
				continue;
			}
		}
		catch (...) {
			fwprintf(stderr, L"could not read at %p, skip...\n", p);
			return;
		}

		DWORD str_len = *(LPDWORD)(p - 4); // unicode len
		if (str_len > 0xff) continue;
		str_len *= 2; // byte len

		std::wstring urlToFile((wchar_t*)p, (wchar_t*)(p + str_len));
		if (urlToFile.ends_with(L".exe") && urlToFile.find(L"/CheatEngine/DOTPS-") != std::string::npos) {
			// We found it?!
			auto hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, _callback_pid);
			fwprintf(stderr, L"found https str: %s\n", urlToFile.c_str());

			if (hProcess == INVALID_HANDLE_VALUE || hProcess == nullptr) {
				fwprintf(stderr, L"could not open process :/\n");
				ExitProcess(1);
			}
			SIZE_T unused;
			bool ok = true;
			WriteProcessMemory(hProcess, _callback_address + 5, p, str_len, &unused);
			WriteProcessMemory(hProcess, _callback_address + 1, &str_len, sizeof(str_len), &unused);
			WriteProcessMemory(hProcess, _callback_address, &ok, sizeof(ok), &unused);
			CloseHandle(hProcess);
			ExitProcess(0);
		}
	}
};


LPBYTE WINAPI MyVirtualAlloc(
	LPBYTE lpAddress,
	SIZE_T dwSize,
	DWORD flAllocationType,
	DWORD flProtect
) {
	AttachConsole(ATTACH_PARENT_PROCESS);
	freopen("CON", "w", stdout);
	freopen("CON", "w", stderr);

#if !NDEBUG
	if (alloc_count == 3) {
		MessageBoxA(nullptr, "Attach me?", "", MB_OK);
	}
#endif

	for (auto item : old_allocs)
	{
		do_search(item.first, item.second);
	}

	if (--alloc_count <= 0) {
		fprintf(stderr, "give up...\n");
		// We probably don't want to continue now...
		ExitProcess(1);
	}

	auto result = real_virtual_alloc(lpAddress, dwSize, flAllocationType, flProtect);
	old_allocs.push_back(std::pair<LPBYTE, SIZE_T>(result, dwSize));
	return result;
}

// This is an example of an exported function.
EXTRACTORDLL_API void __stdcall init_ce_extractor_dll(unsigned int* ptr_to_data)
{
	unsigned int pid = ptr_to_data[0];
	auto address_to_write = (unsigned char*)ptr_to_data[1];
	_callback_pid = pid;
	_callback_address = address_to_write;

	DWORD oldProtect;
	auto entry_virtual_alloc = (BYTE*)(VirtualAlloc);
	auto trampoline_virtual_alloc = entry_virtual_alloc - 5;
	auto my_virtual_alloc = (BYTE*)(MyVirtualAlloc);
	real_virtual_alloc = virtual_alloc_fn(entry_virtual_alloc + 2);

	VirtualProtect(entry_virtual_alloc - 5, 7, PAGE_EXECUTE_READWRITE, &oldProtect);

	// write an inline jump
	*trampoline_virtual_alloc = 0xE9;
	*(LPDWORD)(&trampoline_virtual_alloc[1]) = DWORD(my_virtual_alloc - entry_virtual_alloc);
	*(LPWORD)(entry_virtual_alloc) = 0xF9EB;
	VirtualProtect(entry_virtual_alloc - 5, 7, oldProtect, &oldProtect);
}
