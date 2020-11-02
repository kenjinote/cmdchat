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
	LPWSTR RunCommand(LPCWSTR lpszCommand);
	void NotifyExitProcess();

private:
	HWND hwnd_;
	HANDLE hProcess_;
	HANDLE readPipeStdIn_;
	HANDLE writePipeStdIn_;
	HANDLE readPipeStdOut_;
	HANDLE writePipeStdOut_;

	HANDLE threadProcess_;
};
