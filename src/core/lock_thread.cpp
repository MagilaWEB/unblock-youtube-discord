#include "pch.h"

CriticalSection::CriticalSection()
{
	InitializeCriticalSection(&pmutex);
}

CriticalSection::~CriticalSection()
{
	DeleteCriticalSection(&pmutex);
}

void CriticalSection::Enter()
{
	EnterCriticalSection(&pmutex);
}

void CriticalSection::Leave()
{
	LeaveCriticalSection(&pmutex);
}

BOOL CriticalSection::TryEnter()
{
	return TryEnterCriticalSection(&pmutex);
}

CriticalSection::raii::raii(CriticalSection& other) : critical_section(&std::forward<CriticalSection&>(other))
{
	VERIFY(critical_section);
	critical_section->Enter();
}

CriticalSection::raii::~raii()
{
	critical_section->Leave();
}

FastLock::FastLock()
{
	InitializeSRWLock(&srw);
}

void FastLock::Enter()
{
	AcquireSRWLockExclusive(&srw);
}

bool FastLock::TryEnter()
{
	return 0 != TryAcquireSRWLockExclusive(&srw);
}

void FastLock::Leave()
{
	ReleaseSRWLockExclusive(&srw);
}

void FastLock::EnterShared()
{
	AcquireSRWLockShared(&srw);
}

bool FastLock::TryEnterShared()
{
	return 0 != TryAcquireSRWLockShared(&srw);
}

void FastLock::LeaveShared()
{
	ReleaseSRWLockShared(&srw);
}

void* FastLock::GetHandle()
{
	return reinterpret_cast<void*>(&srw);
}

FastLock::raii::raii(FastLock& other, bool shared) : fast_lock(&std::forward<FastLock&>(other)), _shared(shared)
{
	VERIFY(fast_lock);
	if (_shared)
		fast_lock->EnterShared();
	else
		fast_lock->Enter();
}

FastLock::raii::~raii()
{
	if (_shared)
		fast_lock->LeaveShared();
	else
		fast_lock->Leave();
}
