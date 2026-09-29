#include "pch.h"

CriticalSection::CriticalSection()
{
	InitializeCriticalSection(&_pmutex);
}

CriticalSection::~CriticalSection()
{
	DeleteCriticalSection(&_pmutex);
}

void CriticalSection::enter()
{
	EnterCriticalSection(&_pmutex);
}

void CriticalSection::leave()
{
	LeaveCriticalSection(&_pmutex);
}

BOOL CriticalSection::tryEnter()
{
	return TryEnterCriticalSection(&_pmutex);
}

CriticalSection::Raii::Raii(CriticalSection& other) : _critical_section(&std::forward<CriticalSection&>(other))
{
	VERIFY(_critical_section);
	_critical_section->enter();
}

CriticalSection::Raii::~Raii()
{
	_critical_section->leave();
}

FastLock::FastLock()
{
	InitializeSRWLock(&_srw);
}

void FastLock::enter()
{
	AcquireSRWLockExclusive(&_srw);
}

bool FastLock::tryEnter()
{
	return 0 != TryAcquireSRWLockExclusive(&_srw);
}

void FastLock::leave()
{
	ReleaseSRWLockExclusive(&_srw);
}

void FastLock::enterShared()
{
	AcquireSRWLockShared(&_srw);
}

bool FastLock::tryEnterShared()
{
	return 0 != TryAcquireSRWLockShared(&_srw);
}

void FastLock::leaveShared()
{
	ReleaseSRWLockShared(&_srw);
}

void* FastLock::getHandle()
{
	return reinterpret_cast<void*>(&_srw);
}

FastLock::Raii::Raii(FastLock& other, bool shared) : _fast_lock(&std::forward<FastLock&>(other)), _shared(shared)
{
	VERIFY(_fast_lock);
	if (_shared)
		_fast_lock->enterShared();
	else
		_fast_lock->enter();
}

FastLock::Raii::~Raii()
{
	if (_shared)
		_fast_lock->leaveShared();
	else
		_fast_lock->leave();
}
