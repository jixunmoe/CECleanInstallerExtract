// CECleanExtract.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <Windows.h>
#include <filesystem>
#include "../ExtractorDLL/ExtractorDLL.h"

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct callback_data {
	bool ready;
	DWORD size_of_data;
	wchar_t data[4096];
};

union init_params {
	struct {
		DWORD pid;
		void* ptr;
	} readable;
	char serialised[8];
};
#pragma pack(pop)
callback_data my_callback_data = { 0 };

DWORD run_remote_call(HANDLE hProcess, void* fn, void* param) {
	DWORD threadId;
	auto th = CreateRemoteThread(hProcess, nullptr, 0x100, (LPTHREAD_START_ROUTINE)fn, param, 0, &threadId);

	// Wait for module handle
	WaitForSingleObject(th, INFINITE);
	DWORD result = 0;
	GetExitCodeThread(th, &result);
	return result;
}

int wmain(int argc, wchar_t* argv[])
{
	DWORD pid = GetCurrentProcessId();
	fprintf(stderr, "CE Clean Installer Extractor v0.1 (pid = %lu)\n", pid);
	fprintf(stderr, "\n");

	bool do_skip = argc == 3;

	if (argc < 2) {
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "\n");
		fwprintf(stderr, L"  \"%s\" \"<path to installer>\"\n", argv[0]);
		fwprintf(stderr, L"  \"%s\" \"<path to installer>\" skip-pause\n", argv[0]);
		fprintf(stderr, "\n");
		fprintf(stderr, "Note: This program will invoke the executable passed in. Do not run with un-trusted files.\n");
		fprintf(stderr, "\n");
		return 1;
	}

	PROCESS_INFORMATION pi;
	STARTUPINFOW si = { 0 };
	si.cb = sizeof(si);
	if (!CreateProcessW(nullptr, argv[1], nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
		fprintf(stderr, "CreateProcess failed with error: %lu\n", GetLastError());
		fprintf(stderr, "\n");
	}

	fs::path path = argv[0];
	path = fs::absolute(path.parent_path()) / L"ExtractorDLL.dll";
	auto pathW = path.wstring();

	std::vector<char> payload((char*)pathW.data(), (char*)pathW.data() + pathW.size() * sizeof(pathW[0]));
	payload.push_back(0);
	payload.push_back(0);
	auto init_param_offset = payload.size();
	init_params init_param = { .readable = {.pid = pid, .ptr = &my_callback_data, } };
	for (int i = 0; i < sizeof(init_params); i++) {
		payload.push_back(init_param.serialised[i]);
	}

	auto dll_path_mem = (LPBYTE)VirtualAllocEx(pi.hProcess, nullptr, payload.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	SIZE_T unused;
	WriteProcessMemory(pi.hProcess, dll_path_mem, payload.data(), payload.size(), &unused);

	auto dllHandle = run_remote_call(pi.hProcess, LoadLibraryW, dll_path_mem);
	auto init_param_remote_addr = (LPBYTE)init_ce_extractor_dll - (LPBYTE)LoadLibraryW(pathW.c_str());
	run_remote_call(pi.hProcess, (LPBYTE)dllHandle + init_param_remote_addr, (unsigned int*)(dll_path_mem + init_param_offset));

	ResumeThread(pi.hThread);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	int timeoutCounter = 5000 / 50;
	while (!my_callback_data.ready) {
		fprintf(stderr, ".");
		Sleep(50);

#if !NDEBUG
		if (--timeoutCounter <= 0) {
			break;
		}
#endif
	}

	if (my_callback_data.ready) {
		wprintf(L"\nurl=%s\n", my_callback_data.data);
	} else {
		fprintf(stderr, "ERROR: Timeout\n");
	}

	if (!do_skip) {
		system("pause");
	}

	return 0;
}
