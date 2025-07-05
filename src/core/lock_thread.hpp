#pragma once

// Desc: Simple wrapper for critical section
class CORE_API CriticalSection
{
	CRITICAL_SECTION pmutex;

public:
	struct CORE_API raii
	{
		explicit raii(CriticalSection&);
		~raii();

	private:
		CriticalSection* critical_section;
	};

public:
	CriticalSection();
	~CriticalSection();

	void Enter();
	void Leave();
	BOOL TryEnter();
};

// Non recursive
class CORE_API FastLock
{
	SRWLOCK srw;

public:
	struct CORE_API raii
	{
		explicit raii(FastLock&);
		~raii();

	private:
		FastLock* fast_lock;
	};

	enum EFastLockType : ULONG
	{
		Exclusive = 0,
		Shared	  = CONDITION_VARIABLE_LOCKMODE_SHARED
	};

public:
	FastLock();
	~FastLock() = default;

	void Enter();
	bool TryEnter();
	void Leave();

	void EnterShared();
	bool TryEnterShared();
	void LeaveShared();

	void* GetHandle();
};
