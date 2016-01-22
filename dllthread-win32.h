#include <functional>
#include <stdexcept>
#include <sstream>
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
private:
	native_handle_type m_handle;
	id m_id;

	struct InitStruct {
		std::function<void()> m_fn;
		HANDLE m_dupHandle;
	};

	static DWORD WINAPI threadstart(LPVOID param) {
#if 0
		const char *str1 = "############################### NEW DLLTHREAD #################################\n";
		const char *str2 = "############################### DLLTHREAD FINSHED #############################\n";
		DWORD ret;
		WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), str1, strlen(str1), &ret, NULL);
#endif
		InitStruct *init = reinterpret_cast<InitStruct*>(param);
		init->m_fn();
		SetEvent(init->m_dupHandle);
		CloseHandle(init->m_dupHandle);
		delete init;
#if 0
		WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), str2, strlen(str2), &ret, NULL);
#endif
		return 0;
	}

public:
	dllthread() : m_id(0), m_handle(0) {}

	template<typename Fn, typename ...Args, std::enable_if_t<!std::is_same<std::decay_t<Fn>, dllthread>::value, int> = 0>
	explicit dllthread(Fn&& fn, Args&&... args) :
		m_handle(INVALID_HANDLE_VALUE), m_id(0)
	{
		InitStruct *init = new InitStruct();
		init->m_fn = std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...);
		init->m_dupHandle = INVALID_HANDLE_VALUE;
		try {
			m_handle = CreateEvent(NULL, false, false, NULL);
			if (m_handle == NULL)
				throw std::system_error(0, std::system_category(), "Can't create event");
			HANDLE hProcess = GetCurrentProcess();
			if (!DuplicateHandle(hProcess, m_handle, hProcess, &init->m_dupHandle, 0, FALSE, DUPLICATE_SAME_ACCESS))
				throw std::system_error(0, std::system_category(), "DuplicateHandle failed");
			HANDLE thread = CreateThread(NULL, 0, threadstart, init, 0, &m_id);
			if (thread == NULL)
				throw std::system_error(0, std::system_category(), "Can't start new thread");
			CloseHandle(thread);
		}
		catch (...) {
			CloseHandle(init->m_dupHandle);
			CloseHandle(m_handle);
			delete init;
			throw;
		}
	}

	~dllthread() { if (joinable()) join(); }

	dllthread(dllthread&& other) : m_handle(other.m_handle), m_id(other.m_id) {
		other.m_handle = INVALID_HANDLE_VALUE;
		other.m_id = 0;
	}

	dllthread& operator=(dllthread&& other) {
		if (joinable())
			throw std::runtime_error("This thread is joinable!");
		m_handle = other.m_handle;
		m_id = other.m_id;
		m_handle = INVALID_HANDLE_VALUE;
		m_id = 0;
		return *this;
	}

	dllthread(const dllthread&) = delete;
	dllthread& operator=(const dllthread&) = delete;

	void swap(dllthread& other) {
		std::swap(m_id, other.m_id);
		std::swap(m_handle, other.m_handle);
	}

	bool joinable() { return m_handle != INVALID_HANDLE_VALUE; }

	void join() {
		if (!joinable())
			throw std::runtime_error("This thread is not joinable");
		DWORD ret = WaitForSingleObject(m_handle, INFINITE);
		if (ret != WAIT_OBJECT_0) {
			std::stringstream ss;
			ss << "WaitForSingleObject failed with code " << ret;
			throw std::system_error((int)ret, std::system_category(), ss.str());
		}
		detach();
	}

	void detach() {
		if (!joinable())
			throw std::runtime_error("This thread is null or has been detached already");
		CloseHandle(m_handle);
		m_handle = INVALID_HANDLE_VALUE;
	}

	id get_id() const { return m_id; }
	native_handle_type native_handle() const { return m_handle; }
	static unsigned int hardware_concurrency() { return std::thread::hardware_concurrency(); }
};
