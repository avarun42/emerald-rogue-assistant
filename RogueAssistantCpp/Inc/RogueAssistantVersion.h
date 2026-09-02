#pragma once

#if __has_include("RogueAssistantVersionGenerated.h")
#include "RogueAssistantVersionGenerated.h"
#else
// The legacy Visual Studio DLL remains buildable until the parity gate. CMake
// desktop builds always use the generated header above.
#define ROGUE_ASSISTANT_VERSION_MAJOR 1
#define ROGUE_ASSISTANT_VERSION_MINOR 0
#define ROGUE_ASSISTANT_VERSION_PATCH 0
#define ROGUE_ASSISTANT_VERSION_STRING "1.0.0"
#endif
