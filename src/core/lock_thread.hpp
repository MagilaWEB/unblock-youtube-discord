#pragma once

class CriticalSection
{
	CRITICAL_SECTION _pmutex;

public:
	struct Raii
	{
		explicit Raii(CriticalSection&);
		~Raii();

	private:
		CriticalSection* _critical_section;
	};

public:
	CriticalSection();
	~CriticalSection();

	void enter();
	void leave();
	BOOL tryEnter();
};

// Non recursive
class FastLock
{
	SRWLOCK _srw;

public:
	struct Raii
	{
		Raii(FastLock&, bool shared = false);
		~Raii();

	private:
		FastLock* _fast_lock;
		bool	  _shared{ false };
	};

public:
	FastLock();
	~FastLock() {}

	void enter();
	bool tryEnter();
	void leave();

	void enterShared();
	bool tryEnterShared();
	void leaveShared();

	void* getHandle();
};

#define CRITICAL_SECTION_RAII(_lock, ...)  \
	CriticalSection::Raii mt_##__VA_ARGS__ \
	{                                      \
		_lock                              \
	}

#define FAST_LOCK(_lock, ...)       \
	FastLock::Raii mt_##__VA_ARGS__ \
	{                               \
		_lock                       \
	}
#define FAST_LOCK_SHARED(_lock, ...) \
	FastLock::Raii mt_##__VA_ARGS__  \
	{                                \
		_lock, true                  \
	}
