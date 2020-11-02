#pragma once

#include <windows.h>

class CmdProcess
{
private:
	CmdProcess();
	~CmdProcess();

public:
	static CmdProcess* Get();
	BOOL Create();
	BOOL Exit();
	LPWSTR RunCommand(LPCWSTR lpszCommand);

private:
	HANDLE hProcess_;
	HANDLE readPipeStdIn_;
	HANDLE writePipeStdIn_;
	HANDLE readPipeStdOut_;
	HANDLE writePipeStdOut_;
};
