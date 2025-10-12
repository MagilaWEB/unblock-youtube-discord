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
		raii(FastLock&, bool shared = false);
		~raii();

	private:
		FastLock* fast_lock;
		bool	  _shared{ false };
	};

public:
	FastLock();
	~FastLock() {}

	void Enter();
	bool TryEnter();
	void Leave();

	void EnterShared();
	bool TryEnterShared();
	void LeaveShared();

	void* GetHandle();
};

#define CRITICAL_SECTION_RAII(_lock, ...)  \
	CriticalSection::raii mt_##__VA_ARGS__ \
	{                                      \
		_lock                              \
	}

#define FAST_LOCK(_lock, ...)       \
	FastLock::raii mt_##__VA_ARGS__ \
	{                               \
		_lock                       \
	}
#define FAST_LOCK_SHARED(_lock, ...) \
	FastLock::raii mt_##__VA_ARGS__  \
	{                                \
		_lock, true                  \
	}
