#include "ISyncObject.h"
#include "SingleLock.h"

SingleLock::SingleLock(ISyncObject* syncObject, BOOL lock):
	syncObject_{ syncObject }
{
	if (lock) {
		syncObject_->Lock();
	}
}

SingleLock::~SingleLock()
{
	syncObject_->Unlock();
}

void SingleLock::Lock()
{
	syncObject_->Lock();
}

void SingleLock::Unlock()
{
	syncObject_->Unlock();
}
