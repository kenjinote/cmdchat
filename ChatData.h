#pragma once

#include <list>
#include <string>
#include "CriticalSection.h"

class ChatData
{
private:
	ChatData();
	~ChatData();

public:
	static ChatData* Get();

	const std::wstring& GetPrompt() const;
	void SetPrompt(const std::wstring& prompt);

	BOOL PopFrontOutput(std::wstring* output);
	void PushBackOutput(const std::wstring& output);

private:
	mutable CriticalSection csPrompt_;
	std::wstring prompt_;

	mutable CriticalSection csOutputs_;
	std::list<std::wstring> outputs_;
};