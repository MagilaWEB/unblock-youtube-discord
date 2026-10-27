export module core.lock_thread;

import ext.windows;

export class CriticalSection
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
export class FastLock
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
