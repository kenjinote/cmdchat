#pragma once

#include "ISyncObject.h"

class SyncObject : public ISyncObject
{
protected:
	SyncObject() = default;

public:
	virtual ~SyncObject() = default;

	SyncObject(const SyncObject&) = delete;
	SyncObject(SyncObject&&) = delete;
	SyncObject& operator =(const SyncObject&) = delete;
	SyncObject& operator =(SyncObject&&) = delete;
};
