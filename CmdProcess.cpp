#include <windows.h>
#include <atlbase.h>
#include <atlhost.h>
#include <string>
#include <vector>
#include <winternl.h>
#include "ChatData.h"
#include "CmdProcess.h"

namespace
{
	/******************************************************************************\
	*       This is a part of the Microsoft Source Code Samples.
	*       Copyright 1995 - 1997 Microsoft Corporation.
	*       All rights reserved.
	*       This source code is only intended as a supplement to
	*       Microsoft Development Tools and/or WinHelp documentation.
	*       See these sources for detailed information regarding the
	*       Microsoft samples programs.
	\******************************************************************************/

	/*++
	Copyright (c) 1997  Microsoft Corporation
	Module Name:
		pipeex.c
	Abstract:
		CreatePipe-like function that lets one or both handles be overlapped
	Author:
		Dave Hart  Summer 1997
	Revision History:
	--*/
	BOOL CreatePipeEx(
		LPHANDLE lpReadPipe,
		LPHANDLE lpWritePipe,
		LPSECURITY_ATTRIBUTES lpPipeAttributes,
		DWORD nSize,
		DWORD dwReadMode,
		DWORD dwWriteMode
	)
		/*++
		Routine Description:
			The CreatePipeEx API is used to create an anonymous pipe I/O device.
			Unlike CreatePipe FILE_FLAG_OVERLAPPED may be specified for one or
			both handles.
			Two handles to the device are created.  One handle is opened for
			reading and the other is opened for writing.  These handles may be
			used in subsequent calls to ReadFile and WriteFile to transmit data
			through the pipe.
		Arguments:
			lpReadPipe - Returns a handle to the read side of the pipe.  Data
				may be read from the pipe by specifying this handle value in a
				subsequent call to ReadFile.
			lpWritePipe - Returns a handle to the write side of the pipe.  Data
				may be written to the pipe by specifying this handle value in a
				subsequent call to WriteFile.
			lpPipeAttributes - An optional parameter that may be used to specify
				the attributes of the new pipe.  If the parameter is not
				specified, then the pipe is created without a security
				descriptor, and the resulting handles are not inherited on
				process creation.  Otherwise, the optional security attributes
				are used on the pipe, and the inherit handles flag effects both
				pipe handles.
			nSize - Supplies the requested buffer size for the pipe.  This is
				only a suggestion and is used by the operating system to
				calculate an appropriate buffering mechanism.  A value of zero
				indicates that the system is to choose the default buffering
				scheme.
		Return Value:
			TRUE - The operation was successful.
			FALSE/NULL - The operation failed. Extended error status is available
				using GetLastError.
		--*/
	{
		static volatile LONG pipeSerialNumber = 0L;

		WCHAR PipeNameBuffer[MAX_PATH];

		//
		// Only one valid OpenMode flag - FILE_FLAG_OVERLAPPED
		//

		//if ((dwReadMode | dwWriteMode) & (~FILE_FLAG_OVERLAPPED)) {
		//	SetLastError(ERROR_INVALID_PARAMETER);
		//	return FALSE;
		//}

		//
		//  Set the default timeout to 120 seconds
		//

		if (nSize == 0) {
			nSize = 4096;
		}

		swprintf_s(
			PipeNameBuffer,
			L"\\\\.\\Pipe\\RemoteExeAnon.%08x.%08x",
			GetCurrentProcessId(),
			InterlockedIncrement(&pipeSerialNumber)
		);

		HANDLE ReadPipeHandle = CreateNamedPipeW(
			PipeNameBuffer,
			PIPE_ACCESS_INBOUND | dwReadMode,
			PIPE_TYPE_BYTE | PIPE_WAIT,
			1,             // Number of pipes
			nSize,         // Out buffer size
			nSize,         // In buffer size
			120 * 1000,    // Timeout in ms
			lpPipeAttributes
		);

		if (INVALID_HANDLE_VALUE == ReadPipeHandle) {
			return FALSE;
		}

		HANDLE WritePipeHandle = CreateFileW(
			PipeNameBuffer,
			GENERIC_WRITE,
			0,                         // No sharing
			lpPipeAttributes,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | dwWriteMode,
			NULL                       // Template file
		);

		if (INVALID_HANDLE_VALUE == WritePipeHandle) {
			const DWORD dwError = GetLastError();
			CloseHandle(ReadPipeHandle);
			SetLastError(dwError);
			return FALSE;
		}

		*lpReadPipe = ReadPipeHandle;
		*lpWritePipe = WritePipeHandle;

		return TRUE;
	}

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

	void TrimStringLine(std::wstring* pwstr, BOOL commandLineErase)
	{
		const size_t last = pwstr->find_last_of(L'\n');
		if (last != std::wstring::npos) {
			pwstr->erase(last);
		}

		const size_t first = pwstr->find_first_of(L'\n');
		if (first != std::wstring::npos) {
			if (commandLineErase) {
				pwstr->erase(0, first);
			}
		}
	}

	HANDLE StartThread(_beginthreadex_proc_type startAddress, void* argList)
	{
		return reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, startAddress, argList, 0, nullptr));
	}

}

CmdProcess::CmdProcess() :
	hwnd_{ NULL },
	hProcess_{ NULL },
	readPipeStdIn_{ NULL },
	writePipeStdIn_{ NULL },
	readPipeStdOut_{ NULL },
	writePipeStdOut_{ NULL },
	threadProcess_{ NULL },
	threadReadStdOut_{ NULL },
	eventExit_{ NULL },
	commandLineErase_{ FALSE }
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

	hwnd_ = hwnd;

	SECURITY_ATTRIBUTES sa = { 0 };
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	if (CreatePipeEx(&readPipeStdIn_, &writePipeStdIn_, &sa, 0, 0, 0) == FALSE) {
		return FALSE;
	}

	if (CreatePipeEx(&readPipeStdOut_, &writePipeStdOut_, &sa, 0, FILE_FLAG_OVERLAPPED, 0) == FALSE) {
		return FALSE;
	}

	STARTUPINFOW si = { 0 };
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.hStdOutput = writePipeStdOut_;
	si.hStdInput = readPipeStdIn_;
	si.hStdError = writePipeStdOut_;
	si.wShowWindow = SW_HIDE;

	std::wstring commandLine = szCmdExePath;

	std::vector<wchar_t> commandLineBuffer(commandLine.c_str(), commandLine.c_str() + commandLine.length() + 1);

	PROCESS_INFORMATION pi = { 0 };
	if (CreateProcessW(NULL, &commandLineBuffer.front(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi) == FALSE) {
		return FALSE;
	}

	WaitForSingleObject(pi.hProcess, 1000);

	CloseHandle(pi.hThread);

	hProcess_ = pi.hProcess;

	eventExit_ = CreateEventW(NULL, FALSE, FALSE, NULL);
	if (eventExit_ == NULL) {
		return FALSE;
	}

	threadProcess_ = StartThread(&CmdProcess::ThreadCmdProcess, &hProcess_);
	if (threadProcess_ == NULL) {
		return FALSE;
	}

	threadReadStdOut_ = StartThread(&CmdProcess::ThreadReadStdOut, nullptr);
	if (threadReadStdOut_ == NULL) {
		return FALSE;
	}

	return TRUE;
}

BOOL CmdProcess::Exit()
{
	if (eventExit_) {
		if (threadReadStdOut_) {
			SetEvent(eventExit_);
			WaitForSingleObject(threadReadStdOut_, 30 * 1000);
			threadReadStdOut_ = NULL;
		}
		CloseHandle(eventExit_);
		eventExit_ = NULL;
	}

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

BOOL CmdProcess::RunCommand(LPCWSTR lpszCommand)
{
	if (hProcess_ == NULL) {
		return FALSE;
	}

	if (writePipeStdIn_ == NULL) {
		return FALSE;
	}

	if (!lpszCommand) {
		return FALSE;
	}

	WaitForSingleObject(hProcess_, 1000);

	std::wstring command = lpszCommand;
	command += L"\n";

	std::string writeBuf = ToString(command);
	if (WriteFile(writePipeStdIn_, &writeBuf.front(), static_cast<DWORD>(writeBuf.length()), NULL, NULL) == FALSE) {
		return FALSE;
	}

	commandLineErase_ = TRUE;

	return TRUE;
}

void CmdProcess::NotifyExitProcess()
{
	if (hwnd_) {
		PostMessage(hwnd_, WM_CLOSE, 0, 0);
	}
}

void CmdProcess::ReadStdOut()
{
	HANDLE eventRead = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (eventRead == NULL) {
		return;
	}

	enum
	{
		EVENT_READ,
		EVENT_EXIT,
		EVENT_COUNT
	};

	HANDLE handles[EVENT_COUNT] =
	{
		eventRead,
		eventExit_,
	};

	OVERLAPPED overlapped = { 0 };
	overlapped.hEvent = eventRead;

	WaitForSingleObject(hProcess_, 1000);

	BOOL exit = FALSE;
	BOOL reading = FALSE;

	std::vector<char> buffers;
	while (exit == FALSE) {
		if (ReadFile(readPipeStdOut_, NULL, 0, NULL, &overlapped) == FALSE) {
			if (GetLastError() != ERROR_IO_PENDING) {
				continue;
			}
		}

		const DWORD timeout = reading ? 500 : INFINITE;

		const DWORD dwWaitResult = WaitForMultipleObjects(EVENT_COUNT, handles, FALSE, timeout);
		switch (dwWaitResult) {
		case WAIT_OBJECT_0 + EVENT_READ:
			{
				reading = TRUE;

				DWORD totalBytesAvail = 0;
				if (PeekNamedPipe(readPipeStdOut_, NULL, 0, NULL, &totalBytesAvail, NULL)) {
					if (totalBytesAvail > 0) {
						const size_t size = buffers.size();
						buffers.resize(size + totalBytesAvail);

						DWORD numberOfBytesRead = 0;
						if (ReadFile(readPipeStdOut_, &buffers[size], totalBytesAvail, &numberOfBytesRead, NULL)) {
							buffers.resize(size + numberOfBytesRead);
						}
					}
				}
			}
			break;

		case WAIT_OBJECT_0 + EVENT_EXIT:
			exit = TRUE;
			break;

		case WAIT_TIMEOUT:
			if (reading) {
				reading = FALSE;

				if (buffers.empty() == false) {
					std::string str(buffers.cbegin(), buffers.cend());
					buffers.clear();
					std::wstring wstr = ToWString(str);
					TrimStringLine(&wstr, commandLineErase_);
					if (commandLineErase_ == TRUE) {
						commandLineErase_ = FALSE;
					}

					ChatData::Get()->PushBackOutput(wstr);
					PostMessageW(hwnd_, WM_APP, 0, 0);
				}
			}
			break;

		default:
			break;
		}
	}
}

unsigned int __stdcall CmdProcess::ThreadCmdProcess(void* phProcess)
{
	HANDLE hProcess = *static_cast<HANDLE*>(phProcess);
	WaitForSingleObject(hProcess, INFINITE);
	CmdProcess::Get()->NotifyExitProcess();
	return 0;
}

unsigned int __stdcall CmdProcess::ThreadReadStdOut(void*)
{
	CmdProcess::Get()->ReadStdOut();
	return 0;
}
