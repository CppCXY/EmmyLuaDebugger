#include "emmy_tool.h"
#include <Windows.h>
#include <ImageHlp.h>
#include <iomanip>
#include <iostream>
#include <string>
#include "utility.h"
#include "shme.h"

void SetBreakpoint(HANDLE hProcess, LPVOID entryPoint, bool set, BYTE* data);

bool InjectDllForProcess(HANDLE hProcess, const char* dllDir, const char* dllFileName);

bool StartProcessAndInjectDll(LPCSTR exeFileName,
                              LPSTR command,
                              LPCSTR workDirectory,
                              LPCSTR dllDirectory,
                              LPCSTR dllName,
                              bool blockOnExit
)
{
	PROCESS_INFORMATION processInfo;
	STARTUPINFO startUpInfo{};
	startUpInfo.cb = sizeof(startUpInfo);

	ExeInfo info{};
	if (!GetExeInfo(exeFileName, info) || info.entryPoint == 0)
	{
		//MessageEvent("Error: The entry point for the application could not be located", MessageType_Error);
		return false;
	}

	DWORD flags = DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS | CREATE_NEW_CONSOLE;

	if (!CreateProcess(nullptr,
	                   command,
	                   nullptr,
	                   nullptr,
	                   TRUE,
	                   flags,
	                   nullptr,
	                   workDirectory,
	                   &startUpInfo,
	                   &processInfo))
	{
		//OutputError(GetLastError());
		return false;
	}

	// Running to the entry point currently doesn't work for managed applications, so
	// just start it up.

	if (!info.managed)
	{
		size_t entryPoint = info.entryPoint;

		BYTE breakPointData;
		bool done = false;
		bool hasInject = false;
		bool firstBreakPoint = true;
		bool backToEntry = false;
		while (!done)
		{
			DEBUG_EVENT debugEvent;
			WaitForDebugEvent(&debugEvent, INFINITE);

			DWORD continueStatus = DBG_EXCEPTION_NOT_HANDLED;

			if (debugEvent.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
			{
				if (debugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP ||
					debugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT)
				{
					// windows 初始断点忽略
					if (firstBreakPoint)
					{
						// std::cout << "first breakPoint at: " << (uint64_t)debugEvent.u.Exception.
						// 	ExceptionRecord.ExceptionAddress << std::endl;
						firstBreakPoint = false;
					}
					else if (!hasInject)
					{
						if (reinterpret_cast<size_t>(debugEvent.u.Exception.ExceptionRecord.ExceptionAddress) ==
							entryPoint)
						{
							CONTEXT context;
							context.ContextFlags = CONTEXT_CONTROL;
							GetThreadContext(processInfo.hThread, &context);
#ifdef _WIN64
							context.Rip -= 1;
#else
							context.Eip -= 1;
#endif

							SetThreadContext(processInfo.hThread, &context);

							// Restore the original code bytes.
							SetBreakpoint(processInfo.hProcess, (LPVOID)entryPoint, false, &breakPointData);
							// done = true;

							// Suspend the thread before we continue the debug event so that the program
							// doesn't continue to run.
							SuspendThread(processInfo.hThread);

							// 短暂的停止debug
							DebugActiveProcessStop(processInfo.dwProcessId);

							InjectDllForProcess(processInfo.hProcess, dllDirectory, dllName);

							// output PID for connect
							std::cout << "[PID]" << processInfo.dwProcessId << std::endl;
							std::string connected;
							// block thread
							std::cin >> connected;

							// 恢复debug
							DebugActiveProcess(processInfo.dwProcessId);
							ResumeThread(processInfo.hThread);

							hasInject = true;
							// done = true;
							continueStatus = DBG_CONTINUE;
						}
					}
					else if (!backToEntry)
					{
						if (reinterpret_cast<size_t>(debugEvent.u.Exception.ExceptionRecord.ExceptionAddress) ==
							entryPoint)
						{
							// 此时重新回归到入口函数
							backToEntry = true;
							if (!blockOnExit)
							{
								done = true;
							}
						}
						continueStatus = DBG_CONTINUE;
					}
					else
					{
						// 忽略所有的异常处理，由进程本身的例程处理
						std::cout << "unhandled C breakPoint at: " << (uint64_t)debugEvent.u.Exception.ExceptionRecord.
							ExceptionAddress << std::endl;
					}
					
				}
				else
				{
					// 对于非断点异常,第一次交给进程例程处理，第二次输出错误的信息，让进程自己终止
					if (debugEvent.u.Exception.dwFirstChance == 0)
					{
						switch (debugEvent.u.Exception.ExceptionRecord.ExceptionCode)
						{
						case EXCEPTION_ACCESS_VIOLATION:
							std::cout <<
								"The thread tried to read from or write to a virtual address for which it does not have the appropriate access."
								<< std::endl;
							break;
						case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
							std::cout <<
								"The thread tried to access an array element that is out of bounds and the underlying hardware supports bounds checking."
								<< std::endl;
							break;
						case EXCEPTION_BREAKPOINT:
							std::cout << "A breakpoint was encountered." << std::endl;
							break;
						case EXCEPTION_DATATYPE_MISALIGNMENT:
							std::cout <<
								"The thread tried to read or write data that is misaligned on hardware that does not provide alignment. For example, 16-bit values must be aligned on 2-byte boundaries; 32-bit values on 4-byte boundaries, and so on."
								<< std::endl;
							break;
						case EXCEPTION_FLT_DENORMAL_OPERAND:
							std::cout <<
								"One of the operands in a floating-point operation is denormal. A denormal value is one that is too small to represent as a standard floating-point value."
								<< std::endl;
							break;
						case EXCEPTION_FLT_DIVIDE_BY_ZERO:
							std::cout <<
								"The thread tried to divide a floating-point value by a floating-point divisor of zero."
								<< std::endl;
							break;
						case EXCEPTION_FLT_INEXACT_RESULT:
							std::cout <<
								"The result of a floating-point operation cannot be represented exactly as a decimal fraction."
								<< std::endl;
							break;
						case EXCEPTION_FLT_INVALID_OPERATION:
							std::cout <<
								"This exception represents any floating-point exception not included in this list." <<
								std::endl;
							break;
						case EXCEPTION_FLT_OVERFLOW:
							std::cout <<
								"The exponent of a floating-point operation is greater than the magnitude allowed by the corresponding type."
								<< std::endl;
							break;
						case EXCEPTION_FLT_STACK_CHECK:
							std::cout <<
								"The stack overflowed or underflowed as the result of a floating-point operation." <<
								std::endl;
							break;
						case EXCEPTION_FLT_UNDERFLOW:
							std::cout <<
								"The exponent of a floating-point operation is less than the magnitude allowed by the corresponding type."
								<< std::endl;
							break;
						case EXCEPTION_ILLEGAL_INSTRUCTION:
							std::cout << "The thread tried to execute an invalid instruction." << std::endl;
							break;
						case EXCEPTION_IN_PAGE_ERROR:
							std::cout <<
								"The thread tried to access a page that was not present, and the system was unable to load the page. For example, this exception might occur if a network connection is lost while running a program over the network."
								<< std::endl;
							break;
						case EXCEPTION_INT_DIVIDE_BY_ZERO:
							std::cout << "The thread tried to divide an integer value by an integer divisor of zero." <<
								std::endl;
							break;
						case EXCEPTION_INT_OVERFLOW:
							std::cout <<
								"The result of an integer operation caused a carry out of the most significant bit of the result."
								<< std::endl;
							break;
						case EXCEPTION_INVALID_DISPOSITION:
							std::cout <<
								"An exception handler returned an invalid disposition to the exception dispatcher. Programmers using a high-level language such as C should never encounter this exception."
								<< std::endl;
							break;
						case EXCEPTION_NONCONTINUABLE_EXCEPTION:
							std::cout <<
								"The thread tried to continue execution after a noncontinuable exception occurred." <<
								std::endl;
							break;
						case EXCEPTION_PRIV_INSTRUCTION:
							std::cout <<
								"The thread tried to execute an instruction whose operation is not allowed in the current machine mode."
								<< std::endl;
							break;
						case EXCEPTION_SINGLE_STEP:
							std::cout <<
								"A trace trap or other single-instruction mechanism signaled that one instruction has been executed."
								<< std::endl;
							break;
						default:
							std::cout << "Unknown type of Structured Exception" << std::endl;
						}

						done = true;
					}
				}
			}
			else if (debugEvent.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
			{
				if (blockOnExit)
				{
					SuspendThread(processInfo.hThread);
					std::string breakQuit;
					// block thread
					std::cin >> breakQuit;
				}

				done = true;
			}
			else if (debugEvent.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
			{
				if (!hasInject)
				{
					// Offset the entry point by the load address of the process.
					entryPoint += reinterpret_cast<size_t>(debugEvent.u.CreateProcessInfo.lpBaseOfImage);
					// Write a break point at the entry point of the application so that we
					// will stop when we reach that point.
					SetBreakpoint(processInfo.hProcess, reinterpret_cast<void*>(entryPoint), true, &breakPointData);
				}
				CloseHandle(debugEvent.u.CreateProcessInfo.hFile);
			}
			else if (debugEvent.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT)
			{
				CloseHandle(debugEvent.u.LoadDll.hFile);
			}
			ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, continueStatus);
		}
	}
	DebugActiveProcessStop(processInfo.dwProcessId);

	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);

	return true;
}

void SetBreakpoint(HANDLE hProcess, LPVOID entryPoint, bool set, BYTE* data)
{
	DWORD protection;

	// Give ourself write access to the region.
	if (VirtualProtectEx(hProcess, entryPoint, 1, PAGE_EXECUTE_READWRITE, &protection))
	{
		BYTE buffer[1];

		if (set)
		{
			SIZE_T numBytesRead;
			ReadProcessMemory(hProcess, entryPoint, data, 1, &numBytesRead);

			// Write the int 3 instruction.
			buffer[0] = 0xCC;
		}
		else
		{
			// Restore the original byte.
			buffer[0] = data[0];
		}

		SIZE_T numBytesWritten;
		WriteProcessMemory(hProcess, entryPoint, buffer, 1, &numBytesWritten);

		// Restore the original protections.
		VirtualProtectEx(hProcess, entryPoint, 1, protection, &protection);

		// Flush the cache so we know that our new code gets executed.
		FlushInstructionCache(hProcess, entryPoint, 1);
	}
}

enum MessageType
{
	MessageType_Info = 0,
	MessageType_Warning = 1,
	MessageType_Error = 2
};

void* RemoteDup(HANDLE process, const void* source, size_t length)
{
	void* remote = VirtualAllocEx(process, nullptr, length, MEM_COMMIT, PAGE_READWRITE);
	SIZE_T numBytesWritten;
	WriteProcessMemory(process, remote, source, length, &numBytesWritten);
	return remote;
}

void MessageEvent(const std::string& message, MessageType type = MessageType_Info)
{
	if (type == MessageType_Error)
		std::cerr << "[F]" << message << std::endl;
	else
		std::cout << "[F]" << message << std::endl;
}

void OutputError(DWORD error)
{
	char buffer[1024];
	if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error, 0, buffer, 1024, nullptr))
	{
		std::string message = "Error: ";
		message += buffer;
		MessageEvent(message, MessageType_Error);
	}
}

bool GetStartupDirectory(char* path, int maxPathLength)
{
	if (!GetModuleFileName(nullptr, path, maxPathLength))
	{
		return false;
	}

	char* lastSlash = strrchr(path, '\\');

	if (lastSlash == nullptr)
	{
		return false;
	}

	// Terminate the path after the last slash.

	lastSlash[1] = 0;
	return true;
}

bool ExecuteRemoteKernelFuntion(HANDLE process, const char* functionName, LPVOID param, DWORD& exitCode)
{
	HMODULE kernelModule = GetModuleHandle("Kernel32");
	FARPROC function = GetProcAddress(kernelModule, functionName);

	if (function == nullptr)
	{
		return false;
	}

	DWORD threadId;
	HANDLE thread = CreateRemoteThread(process,
	                                   nullptr,
	                                   0,
	                                   (LPTHREAD_START_ROUTINE)function,
	                                   param,
	                                   0,
	                                   &threadId);

	if (thread != nullptr)
	{
		WaitForSingleObject(thread, INFINITE);
		GetExitCodeThread(thread, &exitCode);

		CloseHandle(thread);
		return true;
	}
	return false;
}

bool IsBeingInjected(DWORD processId, LPCSTR moduleFileName)
{
	HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);

	if (process == nullptr)
	{
		return false;
	}

	bool result = false;

	DWORD exitCode;
	void* remoteFileName = RemoteDup(process, moduleFileName, strlen(moduleFileName) + 1);

	if (ExecuteRemoteKernelFuntion(process, "GetModuleHandleA", remoteFileName, exitCode))
	{
		result = exitCode != 0;
	}

	if (remoteFileName != nullptr)
	{
		VirtualFreeEx(process, remoteFileName, 0, MEM_RELEASE);
	}

	if (process != nullptr)
	{
		CloseHandle(process);
	}
	return result;
}

bool InjectDll(DWORD processId, const char* dllDir, const char* dllFileName)
{
	if (IsBeingInjected(processId, dllFileName))
	{
		MessageEvent("The process already attached.");
		return true;
	}

	MessageEvent("Start inject dll ...");
	bool success = true;

	const HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);

	if (process == nullptr)
	{
		MessageEvent("Failed to open process", MessageType_Error);
		OutputError(GetLastError());
		return false;
	}
	DWORD exitCode = 0;

	// Set dll directory
	void* dllDirRemote = RemoteDup(process, dllDir, strlen(dllDir) + 1);
	ExecuteRemoteKernelFuntion(process, "SetDllDirectoryA", dllDirRemote, exitCode);
	VirtualFreeEx(process, dllDirRemote, 0, MEM_RELEASE);

	// Load the DLL.
	void* remoteFileName = RemoteDup(process, dllFileName, strlen(dllFileName) + 1);
	success &= ExecuteRemoteKernelFuntion(process, "LoadLibraryA", remoteFileName, exitCode);
	VirtualFreeEx(process, remoteFileName, 0, MEM_RELEASE);
	if (!success || exitCode == 0)
	{
		MessageEvent("Failed to load library", MessageType_Error);
		return false;
	}

	// Read shared data & call 'StartupHookMode()'
	TSharedData data;
	if (ReadSharedData(data))
	{
		DWORD threadId;
		HANDLE thread = CreateRemoteThread(process,
		                                   nullptr,
		                                   0,
		                                   (LPTHREAD_START_ROUTINE)data.lpInit,
		                                   nullptr,
		                                   0,
		                                   &threadId);

		if (thread != nullptr)
		{
			WaitForSingleObject(thread, INFINITE);
			GetExitCodeThread(thread, &exitCode);

			CloseHandle(thread);
			success = true;
		}
		else
		{
			success = false;
		}
	}

	// Reset dll directory
	ExecuteRemoteKernelFuntion(process, "SetDllDirectoryA", nullptr, exitCode);

	if (process != nullptr)
	{
		CloseHandle(process);
	}

	if (success)
	{
		MessageEvent("Successfully inject dll!");
	}
	return success;
}

// 和InjectDll 实现不同的是，不会closeHandle
bool InjectDllForProcess(HANDLE hProcess, const char* dllDir, const char* dllFileName)
{
	bool success = true;
	DWORD exitCode = 0;

	// Set dll directory
	void* dllDirRemote = RemoteDup(hProcess, dllDir, strlen(dllDir) + 1);
	ExecuteRemoteKernelFuntion(hProcess, "SetDllDirectoryA", dllDirRemote, exitCode);
	VirtualFreeEx(hProcess, dllDirRemote, 0, MEM_RELEASE);

	// Load the DLL.
	void* remoteFileName = RemoteDup(hProcess, dllFileName, strlen(dllFileName) + 1);
	success &= ExecuteRemoteKernelFuntion(hProcess, "LoadLibraryA", remoteFileName, exitCode);
	VirtualFreeEx(hProcess, remoteFileName, 0, MEM_RELEASE);
	if (!success || exitCode == 0)
	{
		MessageEvent("Failed to load library", MessageType_Error);
		return false;
	}

	// Read shared data & call 'StartupHookMode()'
	TSharedData data;
	if (ReadSharedData(data))
	{
		DWORD threadId;
		HANDLE thread = CreateRemoteThread(hProcess,
		                                   nullptr,
		                                   0,
		                                   (LPTHREAD_START_ROUTINE)data.lpInit,
		                                   nullptr,
		                                   0,
		                                   &threadId);

		if (thread != nullptr)
		{
			WaitForSingleObject(thread, INFINITE);
			GetExitCodeThread(thread, &exitCode);
			CloseHandle(thread);
			success = true;
		}
		else
		{
			success = false;
		}
	}

	// Reset dll directory
	ExecuteRemoteKernelFuntion(hProcess, "SetDllDirectoryA", nullptr, exitCode);

	if (success)
	{
		MessageEvent("Successfully inject dll!");
	}
	return success;
}
