#include "Log.h"

#include <cstdarg>
#include <ctime>
#include <mutex>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static std::mutex s_LogMutex;
static FILE* s_LogFile = nullptr;
static bool s_LogFileAttempted = false;

void RogueLog_Write(char const* level, char const* format, ...)
{
	char message[1024];

	va_list args;
	va_start(args, format);
	int written = vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	if (written < 0)
		return;

	// Called from the window thread, the connection threads and the emulator Lua
	// thread, so serialise it.
	std::lock_guard<std::mutex> lock(s_LogMutex);

	if (!s_LogFileAttempted)
	{
		s_LogFileAttempted = true;
		fopen_s(&s_LogFile, "RogueAssistant.log", "w");
	}

	time_t now = time(nullptr);
	tm local;
	char stamp[32] = "";
	if (localtime_s(&local, &now) == 0)
		strftime(stamp, sizeof(stamp), "%H:%M:%S", &local);

	unsigned long threadId = GetCurrentThreadId();

	fprintf(stderr, "[%s][%s][t%lu] %s\n", stamp, level, threadId, message);
	fflush(stderr);

	if (s_LogFile != nullptr)
	{
		fprintf(s_LogFile, "[%s][%s][t%lu] %s\n", stamp, level, threadId, message);
		fflush(s_LogFile); // we're logging to survive a hard crash, so don't buffer
	}

	if (IsDebuggerPresent())
	{
		char debugLine[1152];
		snprintf(debugLine, sizeof(debugLine), "[%s][t%lu] %s\n", level, threadId, message);
		OutputDebugStringA(debugLine);
	}
}
