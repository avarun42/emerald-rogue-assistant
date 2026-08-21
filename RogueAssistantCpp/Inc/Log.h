#pragma once
#include <cstdio>

#ifdef _DEBUG
#define _ASSERTS
#endif

// Logging is deliberately enabled in Release as well.
//
// This DLL runs inside the emulator process, so when something goes wrong the
// emulator simply disappears - no dialog, no dump, nothing in the event log.
// Previously LOG_* and ASSERT_* were compiled out entirely for Release, which
// meant every packet-size and bounds check in the shipping build was a no-op and
// there was no trail at all. Output goes to stderr, the debugger, and
// RogueAssistant.log in the working directory.
void RogueLog_Write(char const* level, char const* format, ...);

#define LOG_INFO(...)  RogueLog_Write("INFO", __VA_ARGS__)
#define LOG_WARN(...)  RogueLog_Write("WARN", __VA_ARGS__)
#define LOG_ERROR(...) RogueLog_Write("ERROR", __VA_ARGS__)

#ifdef _ASSERTS
#define ASSERT_MSG(condition, ...) do { if(!(condition)) { LOG_ERROR(__VA_ARGS__); __debugbreak(); } } while(0)
#else
// Still evaluated and still logged - just doesn't break into the debugger.
#define ASSERT_MSG(condition, ...) do { if(!(condition)) { LOG_ERROR(__VA_ARGS__); } } while(0)
#endif

#define ASSERT_FAIL(...) ASSERT_MSG(false, __VA_ARGS__)
