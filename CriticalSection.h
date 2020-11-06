#pragma once

#include <Windows.h>
#include "SyncObject.h"

class CriticalSection : public SyncObject
{
public:
	CriticalSection();
	virtual ~CriticalSection() override;

	virtual void Lock() override;
	virtual void Unlock() override;

private:
	CRITICAL_SECTION criticalSection_;
};
