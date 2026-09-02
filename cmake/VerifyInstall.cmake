foreach(required_variable IN ITEMS
        ROGUE_INSTALL_ROOT
        ROGUE_INSTALL_PLATFORM
        ROGUE_EXPECTED_VERSION)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${required_variable} is required")
  endif()
endforeach()

if(ROGUE_INSTALL_PLATFORM STREQUAL "windows")
  set(executable ${ROGUE_INSTALL_ROOT}/bin/RogueAssistant.exe)
  set(resource_directory ${ROGUE_INSTALL_ROOT}/bin/resources)
elseif(ROGUE_INSTALL_PLATFORM STREQUAL "macos")
  set(executable ${ROGUE_INSTALL_ROOT}/RogueAssistant.app/Contents/MacOS/RogueAssistant)
  set(resource_directory ${ROGUE_INSTALL_ROOT}/RogueAssistant.app/Contents/Resources)
  set(document_directory ${resource_directory}/Documentation)
elseif(ROGUE_INSTALL_PLATFORM STREQUAL "linux")
  set(executable ${ROGUE_INSTALL_ROOT}/bin/RogueAssistant)
  set(resource_directory ${ROGUE_INSTALL_ROOT}/bin/resources)
else()
  message(FATAL_ERROR "Unsupported install platform: ${ROGUE_INSTALL_PLATFORM}")
endif()

if(NOT DEFINED document_directory)
  set(document_directory ${ROGUE_INSTALL_ROOT}/share/doc/RogueAssistant)
endif()

function(verify_directory_inventory directory)
  set(expected_entries ${ARGN})
  file(GLOB actual_entries LIST_DIRECTORIES true RELATIVE "${directory}" "${directory}/*")
  list(SORT expected_entries)
  list(SORT actual_entries)
  if(NOT "${actual_entries}" STREQUAL "${expected_entries}")
    message(
      FATAL_ERROR
      "Unexpected package inventory in ${directory}\n"
      "Expected: ${expected_entries}\n"
      "Actual:   ${actual_entries}"
    )
  endif()
endfunction()

set(
  required_files
  ${executable}
  ${resource_directory}/WobbuffetImage.png
  ${resource_directory}/WobbuffetIcon.ico
  ${resource_directory}/poketch_frame.png
  ${resource_directory}/pokemon-emerald-pro.ttf
  ${resource_directory}/RogueAssistant_mGBA.lua
  ${document_directory}/README.md
  ${document_directory}/THIRD_PARTY_NOTICES.md
  ${document_directory}/docs/architecture.md
  ${document_directory}/docs/asset-provenance.md
  ${document_directory}/docs/bridge-protocol.md
  ${document_directory}/docs/compatibility.md
  ${document_directory}/docs/desktop-application.md
  ${document_directory}/docs/development.md
  ${document_directory}/docs/game-memory-transport.md
  ${document_directory}/docs/home-box-format.md
  ${document_directory}/docs/installation.md
  ${document_directory}/docs/lifecycle.md
  ${document_directory}/docs/multiplayer-protocol.md
  ${document_directory}/docs/parity-gate.md
  ${document_directory}/docs/platform-services.md
  ${document_directory}/docs/release.md
  ${document_directory}/docs/roadmap.md
  ${document_directory}/licenses/SFML.txt
  ${document_directory}/licenses/ENet.txt
  ${document_directory}/licenses/FreeType.txt
  ${document_directory}/licenses/HarfBuzz.txt
  ${document_directory}/licenses/SheenBidi.txt
)
foreach(required_file IN LISTS required_files)
  if(NOT EXISTS ${required_file})
    message(FATAL_ERROR "Installed package is missing: ${required_file}")
  endif()
endforeach()

verify_directory_inventory(
  "${document_directory}"
  README.md
  THIRD_PARTY_NOTICES.md
  docs
  licenses
)
verify_directory_inventory(
  "${document_directory}/docs"
  architecture.md
  asset-provenance.md
  bridge-protocol.md
  compatibility.md
  desktop-application.md
  development.md
  game-memory-transport.md
  home-box-format.md
  installation.md
  lifecycle.md
  multiplayer-protocol.md
  parity-gate.md
  platform-services.md
  release.md
  roadmap.md
)
verify_directory_inventory(
  "${document_directory}/licenses"
  ENet.txt
  FreeType.txt
  HarfBuzz.txt
  SFML.txt
  SheenBidi.txt
)
if(ROGUE_INSTALL_PLATFORM STREQUAL "macos")
  verify_directory_inventory(
    "${resource_directory}"
    Documentation
    RogueAssistant.icns
    RogueAssistant_mGBA.lua
    WobbuffetIcon.ico
    WobbuffetImage.png
    pokemon-emerald-pro.ttf
    poketch_frame.png
  )
else()
  verify_directory_inventory(
    "${resource_directory}"
    RogueAssistant_mGBA.lua
    WobbuffetIcon.ico
    WobbuffetImage.png
    pokemon-emerald-pro.ttf
    poketch_frame.png
  )
endif()

execute_process(
  COMMAND ${executable} --version
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
  TIMEOUT 10
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Installed executable failed --version (${result}): ${error}")
endif()
string(STRIP "${output}" output)
if(NOT output STREQUAL "Rogue Assistant ${ROGUE_EXPECTED_VERSION}")
  message(FATAL_ERROR "Unexpected installed version output: ${output}")
endif()
