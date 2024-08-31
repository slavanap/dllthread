#pragma once

#ifdef _WIN32

#ifndef _WINDOWS_
#	define WIN32_LEAN_AND_MEAN
#	define NOMINMAX
#	include <Windows.h>
#endif

#include <functional>
#include <system_error>
#include <thread>


// Unitily class. Similar to recursive_mutex, can be safely used during Dll initialization
class RTLSection {
public:
	typedef RTL_CRITICAL_SECTION* native_handle_type;

	RTLSection() { InitializeCriticalSection(&cs); }
	~RTLSection() { DeleteCriticalSection(&cs); }
	RTLSection(const RTLSection&) = delete;
	RTLSection& operator=(const RTLSection&) = delete;

	void lock() { EnterCriticalSection(&cs); }
	bool try_lock() { return !!TryEnterCriticalSection(&cs); }
	void unlock() { LeaveCriticalSection(&cs); }
	native_handle_type native_handle() { return &cs; }
private:
	RTL_CRITICAL_SECTION cs;
};

class dllthread {
public:
	typedef DWORD id;
	typedef HANDLE native_handle_type;

	id get_id() const { return m_id; }
	native_handle_type native_handle() const { return m_thread; }
	static unsigned int hardware_concurrency() { return std::thread::hardware_concurrency(); }
	
	dllthread() { reset(); }

	template<typename Fn, typename ...Args, typename std::enable_if<!std::is_same<typename std::decay<Fn>::type, dllthread>::value, int>::type* = nullptr>
	explicit dllthread(Fn&& fn, Args&&... args) {
		reset();
		init(std::move(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...)));
	}

	~dllthread() {
		if (joinable())
			join();
	}

	dllthread(const dllthread&) = delete;
	dllthread& operator=(const dllthread&) = delete;

	dllthread(dllthread&& other) :
		m_threadStarted(other.m_threadStarted),
		m_thread(other.m_thread),
		m_id(other.m_id),
		m_initstruct(other.m_initstruct)
	{
		other.reset();
	}

	dllthread& operator=(dllthread&& other) {
		if (joinable())
			join();
		m_threadStarted = other.m_threadStarted;
		m_id = other.m_id;
		m_thread = other.m_thread;
		m_initstruct = other.m_initstruct;
		other.reset();
		return *this;
	}

	void swap(dllthread& other) {
		std::swap(m_threadStarted, other.m_threadStarted);
		std::swap(m_thread, other.m_thread);
		std::swap(m_id, other.m_id);
		std::swap(m_initstruct. other.m_initstruct);
	}

	bool joinable() const {
		return m_thread != INVALID_HANDLE_VALUE;
	}

	void join();
	void detach();

private:
	HANDLE m_threadStarted, m_thread;
	DWORD m_id;

	struct InitStruct;
	InitStruct *m_initstruct; // in case for deadlock
	static DWORD WINAPI threadstart(LPVOID param);

	void init(std::function<void()>&& fn);
	void reset() {
		m_threadStarted = INVALID_HANDLE_VALUE;
		m_thread = INVALID_HANDLE_VALUE;
		m_id = 0;
		m_initstruct = nullptr;
	}
};

#else // ifdef _WIN32

#include <thread>
typedef std::thread dllthread;

#endif // ifdef _WIN32
