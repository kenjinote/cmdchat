#pragma once

#include <Windows.h>

class ISyncObject;

class SingleLock
{
public:
	SingleLock() = delete;
	SingleLock(const SingleLock&) = delete;
	SingleLock(SingleLock&&) = delete;
	SingleLock& operator =(const SingleLock&) = delete;
	SingleLock& operator =(SingleLock&&) = delete;

	explicit SingleLock(ISyncObject* syncObject, BOOL lock = TRUE);
	~SingleLock();

	void Lock();
	void Unlock();

private:
	ISyncObject* syncObject_;
};

