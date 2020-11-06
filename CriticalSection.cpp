#include "CriticalSection.h"

CriticalSection::CriticalSection() :
	criticalSection_{}
{
	InitializeCriticalSection(&criticalSection_);
}

CriticalSection::~CriticalSection()
{
}

void CriticalSection::Lock()
{
	EnterCriticalSection(&criticalSection_);
}

void CriticalSection::Unlock()
{
	LeaveCriticalSection(&criticalSection_);
}