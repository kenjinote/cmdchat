#include <windows.h>
#include <atlbase.h>
#include <atlhost.h>
#include <string>
#include <vector>
#include <winternl.h>
#include "CmdProcess.h"

namespace
{
	std::wstring ToWString(const std::string& str)
	{
		const int size = MultiByteToWideChar(CP_THREAD_ACP, 0, str.c_str(), static_cast<int>(str.length()), NULL, 0);

		std::wstring wstr;
		if (size > 0)
		{
			std::vector<wchar_t> buffers(size, L'\0');
			MultiByteToWideChar(CP_THREAD_ACP, 0, str.c_str(), static_cast<int>(str.length()), &buffers.front(), size);
			wstr.assign(buffers.cbegin(), buffers.cend());
		}

		return wstr;
	}

	std::string ToString(const std::wstring& wstr)
	{
		const int size = WideCharToMultiByte(CP_THREAD_ACP, 0, wstr.c_str(), static_cast<int>(wstr.length()), NULL, 0, NULL, NULL);

		std::string str;
		if (size > 0)
		{
			std::vector<char> buffers(size, '\0');
			WideCharToMultiByte(CP_THREAD_ACP, 0, wstr.c_str(), static_cast<int>(wstr.length()), &buffers.front(), size, NULL, NULL);
			str.assign(buffers.cbegin(), buffers.cend());
		}

		return str;
	}

	void TrimStringLine(std::wstring* pwstr)
	{
		const size_t last = pwstr->find_last_of(L'\n');
		if (last != std::wstring::npos) {
			pwstr->erase(last);
		}

		const size_t first = pwstr->find_first_of(L'\n');
		if (first != std::wstring::npos) {
			pwstr->erase(0, first);
		}
	}

	HANDLE StartThread(_beginthreadex_proc_type startAddress, void* argList)
	{
		return reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, startAddress, argList, 0, nullptr));
	}

	unsigned __stdcall ThreadCmdProcess(void* phProcess)
	{
		HANDLE hProcess = reinterpret_cast<HANDLE*>(phProcess);
		WaitForSingleObject(hProcess, INFINITE);
		CmdProcess::Get()->NotifyExitProcess();
		return 0;
	}
}

CmdProcess::CmdProcess() :
	hwnd_{ NULL },
	hProcess_{ NULL },
	readPipeStdIn_{ NULL },
	writePipeStdIn_{ NULL },
	readPipeStdOut_{ NULL },
	writePipeStdOut_{ NULL },
	threadProcess_{ NULL }
{
}

CmdProcess::~CmdProcess()
{
	Exit();
}

CmdProcess* CmdProcess::Get()
{
	static CmdProcess singleton;
	return &singleton;
}

BOOL CmdProcess::Create(HWND hwnd)
{
	WCHAR szCmdExePath[MAX_PATH] = { 0 };
	if (GetEnvironmentVariableW(L"ComSpec", szCmdExePath, _countof(szCmdExePath)) == 0)
	{
		return FALSE;
	}

	HANDLE readPipeStdIn = NULL;
	HANDLE writePipeStdIn = NULL;
	HANDLE readPipeStdOut = NULL;
	HANDLE writePipeStdOut = NULL;

	SECURITY_ATTRIBUTES sa = { 0 };
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	if (CreatePipe(&readPipeStdIn, &writePipeStdIn, &sa, 0) == FALSE) {
		return FALSE;
	}

	if (CreatePipe(&readPipeStdOut, &writePipeStdOut, &sa, 0) == FALSE) {
		CloseHandle(readPipeStdIn);
		CloseHandle(writePipeStdIn);
		return FALSE;
	}

	STARTUPINFOW si = { 0 };
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.hStdOutput = writePipeStdOut;
	si.hStdInput = readPipeStdIn;
	si.hStdError = writePipeStdOut;
	si.wShowWindow = SW_HIDE;

	std::wstring commandLine = szCmdExePath;

	std::vector<wchar_t> commandLineBuffer(commandLine.c_str(), commandLine.c_str() + commandLine.length() + 1);

	PROCESS_INFORMATION pi = { 0 };
	if (CreateProcessW(NULL, &commandLineBuffer.front(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi) == FALSE) {
		CloseHandle(readPipeStdIn);
		CloseHandle(writePipeStdIn);
		CloseHandle(readPipeStdOut);
		CloseHandle(writePipeStdOut);
		return FALSE;
	}

	WaitForSingleObject(pi.hProcess, 1000);

	CloseHandle(pi.hThread);

	HANDLE threadProcess = StartThread(&ThreadCmdProcess, pi.hProcess);
	if (threadProcess == NULL) {
		CloseHandle(readPipeStdIn);
		CloseHandle(writePipeStdIn);
		CloseHandle(readPipeStdOut);
		CloseHandle(writePipeStdOut);
		return FALSE;
	}

	hwnd_ = hwnd;
	hProcess_ = pi.hProcess;
	readPipeStdIn_ = readPipeStdIn;
	writePipeStdIn_ = writePipeStdIn;
	readPipeStdOut_ = readPipeStdOut;
	writePipeStdOut_ = writePipeStdOut;
	threadProcess_ = threadProcess;

	return TRUE;
}

BOOL CmdProcess::Exit()
{
	if (hwnd_) {
		hwnd_ = NULL;
	}

	if (readPipeStdIn_) {
		CloseHandle(readPipeStdIn_);
		readPipeStdIn_ = NULL;
	}

	if (writePipeStdIn_) {
		CloseHandle(writePipeStdIn_);
		writePipeStdIn_ = NULL;
	}

	if (readPipeStdOut_) {
		CloseHandle(readPipeStdOut_);
		readPipeStdOut_ = NULL;
	}

	if (writePipeStdOut_) {
		CloseHandle(writePipeStdOut_);
		writePipeStdOut_ = NULL;
	}

	if (hProcess_)
	{
		TerminateProcess(hProcess_, 0);
		CloseHandle(hProcess_);
		hProcess_ = NULL;
	}

	return TRUE;
}

LPWSTR CmdProcess::RunCommand(LPCWSTR lpszCommand)
{
	if (hProcess_ == NULL) {
		return nullptr;
	}

	if (readPipeStdIn_ == NULL) {
		return nullptr;
	}

	if (writePipeStdIn_ == NULL) {
		return nullptr;
	}

	if (readPipeStdOut_ == NULL) {
		return nullptr;
	}

	if (writePipeStdOut_ == NULL) {
		return nullptr;
	}

	if (!lpszCommand) {
		return nullptr;
	}

	WaitForSingleObject(hProcess_, 1000);

	std::wstring command = lpszCommand;
	command += L"\n";

	std::string writeBuf = ToString(command);
	if (WriteFile(writePipeStdIn_, &writeBuf.front(), static_cast<DWORD>(writeBuf.length()), NULL, NULL) == FALSE) {
		return nullptr;
	}

	LPWSTR lpszReturn = L"";

	CHAR readBuf[1025];
	std::string str;
	BOOL end = FALSE;
	do {
		WaitForSingleObject(hProcess_, 1000);
		DWORD totalLen, len;
		if (PeekNamedPipe(readPipeStdOut_, NULL, 0, NULL, &totalLen, NULL) && totalLen > 0) {
			if (ReadFile(readPipeStdOut_, readBuf, sizeof(readBuf) - 1, &len, NULL) && len > 0) {
				readBuf[len] = '\0';
				str += readBuf;
			}
		}
		else
		{
			end = TRUE;
		}
	} while (!end);
	if (str.empty() == false) {
		std::wstring wstr = ToWString(str);
		TrimStringLine(&wstr);
		lpszReturn = static_cast<LPWSTR>(GlobalAlloc(0, (wstr.length() + 1) * sizeof(WCHAR)));
		if (lpszReturn) {
			wcsncpy_s(lpszReturn, wstr.length() + 1, wstr.c_str(), wstr.length() + 1);
		}
	}

	return lpszReturn;
}

void CmdProcess::NotifyExitProcess()
{
	if (hwnd_) {
		PostMessage(hwnd_, WM_CLOSE, 0, 0);
	}
}