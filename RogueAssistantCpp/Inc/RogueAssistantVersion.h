#pragma once

#if __has_include("RogueAssistantVersionGenerated.h")
#include "RogueAssistantVersionGenerated.h"
#else
// Tooling that parses a source file without a configured build still sees the
// current public version. Production builds always use the generated header.
#define ROGUE_ASSISTANT_VERSION_MAJOR 1
#define ROGUE_ASSISTANT_VERSION_MINOR 0
#define ROGUE_ASSISTANT_VERSION_PATCH 0
#define ROGUE_ASSISTANT_VERSION_STRING "1.0.0"
#endif
