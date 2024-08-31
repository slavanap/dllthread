#include "dllthread.hpp"


struct dllthread::InitStruct {
	std::function<void()> m_fn;
	HANDLE m_threadStarted;
	InitStruct(std::function<void()>&& fn) :
		m_fn(std::move(fn)),
		m_threadStarted(INVALID_HANDLE_VALUE)
	{
	}
	~InitStruct() {
		CloseHandle(m_threadStarted);
		m_threadStarted = INVALID_HANDLE_VALUE;
	}
};

void dllthread::join() {
	if (!joinable())
		throw std::invalid_argument("This thread is not joinable");
	if (GetCurrentThreadId() == m_id)
		throw std::runtime_error("Can't join. Possible deadlock.");

	// this is the most complex part
	// first of all we need to check whether the thread was started.
	// I give thread up to 3 seconds to start, if thread is not started in this time,
	// then most probably we are in DllMain function, thus, we are in deadlock situation, so
	// we need to emulate successfull thread join via thread termination.
	HANDLE handles[2] = { m_threadStarted, m_thread };
	auto ret = WaitForMultipleObjects(sizeof(handles) / sizeof(*handles), handles, FALSE, 3000);
	switch (ret) {
		// m_threadStarted
		case WAIT_OBJECT_0:
			// thread started thus, going to default routine
			WaitForSingleObject(m_thread, INFINITE);
			break;

		// m_thread
		case (WAIT_OBJECT_0 + 1) :
			// success, thread run & finished
			break;

		case WAIT_TIMEOUT:
			// error, but we pretend that it is success, because thread wasn't even started yet.
			// force thread termination and pretend we are ok.
			TerminateThread(m_thread, 0);
			delete m_initstruct;
			break;
	}
	detach();
}

void dllthread::detach() {
	if (!joinable())
		throw std::runtime_error("This thread is null or has been detached already");
	CloseHandle(m_thread);
	CloseHandle(m_threadStarted);
	reset(*this);
}

DWORD WINAPI dllthread::threadstart(LPVOID param) {
	std::function<void()> fn;
	{
		auto init = reinterpret_cast<InitStruct*>(param);
		SetEvent(init->m_threadStarted);
		fn = std::move(init->m_fn);
		delete init;
	}
	fn();
	return 0;
}

void dllthread::init(std::function<void()>&& fn) {
	auto init = new InitStruct(std::move(fn));
	m_initstruct = init;
	try {
		if ((m_threadStarted = CreateEvent(nullptr, false, false, nullptr)) == nullptr)
		{
			m_threadStarted = INVALID_HANDLE_VALUE;
			throw std::system_error(0, std::system_category(), "Can't create event");
		}
		HANDLE hProcess = GetCurrentProcess();
		if (!DuplicateHandle(hProcess, m_threadStarted, hProcess, &init->m_threadStarted, 0, FALSE, DUPLICATE_SAME_ACCESS))
			throw std::system_error(0, std::system_category(), "DuplicateHandle failed");
		m_thread = CreateThread(nullptr, 0, threadstart, init, 0, &m_id);
		if (m_thread == nullptr) {
			m_thread = INVALID_HANDLE_VALUE;
			throw std::system_error(0, std::system_category(), "Can't start new thread");
		}
	}
	catch (...) {
		CloseHandle(m_thread);
		CloseHandle(m_threadStarted);
		delete init;
		throw;
	}
}
