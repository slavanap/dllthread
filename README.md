# dllthread
Provides Win32 std::thread analogue class that allows to create threads safely in DLLMain function (standard std::thread creation in DLLMain will result in a deadlock).
