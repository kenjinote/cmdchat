#pragma once

class ISyncObject
{
public:
	virtual void Lock() = 0;
	virtual void Unlock() = 0;
};
