#pragma once

#include <windows.h>

class CmdProcess
{
private:
	CmdProcess();
	~CmdProcess();

public:
	static CmdProcess* Get();
	BOOL Create(HWND hwnd);
	BOOL Exit();
	BOOL RunCommand(LPCWSTR lpszCommand);

private:
	void NotifyExitProcess();
	void ReadStdOut();
	static unsigned int __stdcall ThreadCmdProcess(void* phProcess);
	static unsigned int __stdcall ThreadReadStdOut(void*);

private:
	HWND hwnd_;
	HANDLE hProcess_;
	HANDLE readPipeStdIn_;
	HANDLE writePipeStdIn_;
	HANDLE readPipeStdOut_;
	HANDLE writePipeStdOut_;

	HANDLE threadProcess_;
	HANDLE threadReadStdOut_;
	HANDLE eventExit_;

	BOOL commandLineErase_;
};
