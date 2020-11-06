#include "SingleLock.h"
#include "ChatData.h"

ChatData::ChatData()
{
}

ChatData::~ChatData()
{
}

ChatData* ChatData::Get()
{
	static ChatData chatData;
	return &chatData;
}

const std::wstring& ChatData::GetPrompt() const
{
	SingleLock sl(&csPrompt_);

	return prompt_;
}

void ChatData::SetPrompt(const std::wstring& prompt)
{
	SingleLock sl(&csPrompt_);

	prompt_ = prompt;
}

BOOL ChatData::PopFrontOutput(std::wstring* output)
{
	SingleLock sl(&csOutputs_);

	if (outputs_.empty()) {
		output->clear();
		return FALSE;
	}

	*output = outputs_.front();
	outputs_.pop_front();
	return TRUE;
}

void ChatData::PushBackOutput(const std::wstring& output)
{
	SingleLock sl(&csOutputs_);

	outputs_.push_back(output);
}
