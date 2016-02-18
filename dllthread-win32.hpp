#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <functional>
#include <system_error>
#include <thread>

// Unitily class. Similar to recursive_mutex, can be safely used during Dll initialization
class RTLSection {
private:
	RTL_CRITICAL_SECTION cs;
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
};

class dllthread {
public:
	typedef DWORD id;
	typedef HANDLE native_handle_type;

	id get_id() const { return m_id; }
	native_handle_type native_handle() const { return m_handle; }
	static unsigned int hardware_concurrency() { return std::thread::hardware_concurrency(); }
	
	dllthread() { reset(*this); }

	template<typename Fn, typename ...Args, std::enable_if_t<!std::is_same<std::decay_t<Fn>, dllthread>::value, int> = 0>
	explicit dllthread(Fn&& fn, Args&&... args) {
		reset(*this);
		init(std::move(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...)));
	}

	~dllthread() { 
		if (joinable())
			join();
	}

	dllthread(const dllthread&) = delete;
	dllthread& operator=(const dllthread&) = delete;

	dllthread(dllthread&& other) :
		m_handle(other.m_handle), m_thread(other.m_thread), m_id(other.m_id)
	{
		reset(other);
	}

	dllthread& operator=(dllthread&& other) {
		if (joinable())
			join();
		m_handle = other.m_handle;
		m_id = other.m_id;
		m_thread = other.m_thread;
		reset(other);
		return *this;
	}

	void swap(dllthread& other) {
		std::swap(m_handle, other.m_handle);
		std::swap(m_thread, other.m_thread);
		std::swap(m_id, other.m_id);
	}

	bool joinable() const {
		return m_handle != INVALID_HANDLE_VALUE;
	}

	void join();
	void detach();

private:
	HANDLE m_handle, m_thread;
	DWORD m_id;

	void init(std::function<void()>&& fn);
	static void reset(dllthread& obj);
};