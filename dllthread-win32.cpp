#include "dllthread-win32.hpp"

void dllthread::join() {
	if (!joinable())
		throw std::runtime_error("This thread is not joinable");
	// check m_thread handle first (thread might be interrupted by system before)
	// and if thread is running then wait for the m_handle
	if (WaitForSingleObject(m_thread, 0) == WAIT_TIMEOUT)
		WaitForSingleObject(m_handle, INFINITE);
	detach();
}

void dllthread::detach() {
	if (!joinable())
		throw std::runtime_error("This thread is null or has been detached already");
	CloseHandle(m_thread);
	CloseHandle(m_handle);
	reset(*this);
}

struct InitStruct {
	std::function<void()> m_fn;
	HANDLE m_dupHandle;
	InitStruct(std::function<void()>&& fn) : m_fn(std::move(fn)), m_dupHandle(INVALID_HANDLE_VALUE) {}
};

static DWORD WINAPI threadstart(LPVOID param) {
#if 0
	const char *str1 = "############################### NEW DLLTHREAD #################################\n";
	const char *str2 = "############################### DLLTHREAD FINSHED #############################\n";
	DWORD ret;
	WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), str1, strlen(str1), &ret, NULL);
#endif
	auto init = reinterpret_cast<InitStruct*>(param);
	init->m_fn();
	SetEvent(init->m_dupHandle);
	CloseHandle(init->m_dupHandle);
	delete init;
#if 0
	WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), str2, strlen(str2), &ret, NULL);
#endif
	return 0;
}

void dllthread::init(std::function<void()>&& fn) {
	auto init = new InitStruct(std::move(fn));
	try {
		m_handle = CreateEvent(NULL, false, false, NULL);
		if (m_handle == NULL) {
			m_handle = INVALID_HANDLE_VALUE;
			throw std::system_error(0, std::system_category(), "Can't create event");
		}
		HANDLE hProcess = GetCurrentProcess();
		if (!DuplicateHandle(hProcess, m_handle, hProcess, &init->m_dupHandle, 0, FALSE, DUPLICATE_SAME_ACCESS))
			throw std::system_error(0, std::system_category(), "DuplicateHandle failed");
		m_thread = CreateThread(NULL, 0, threadstart, init, 0, &m_id);
		if (m_thread == NULL) {
			m_thread = INVALID_HANDLE_VALUE;
			throw std::system_error(0, std::system_category(), "Can't start new thread");
		}
	}
	catch (...) {
		CloseHandle(m_thread);
		CloseHandle(init->m_dupHandle);
		CloseHandle(m_handle);
		delete init;
		throw;
	}
}

void dllthread::reset(dllthread& obj) {
	obj.m_handle = INVALID_HANDLE_VALUE;
	obj.m_thread = INVALID_HANDLE_VALUE;
	obj.m_id = 0;
}
