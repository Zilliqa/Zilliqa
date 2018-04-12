# ==========================================================================
# 2015 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
# with no warranties. This code is yours to share, use and modify with no
# strings attached and no restrictions or obligations.
# 
# For more information see g3log/LICENSE or refer refer to http://unlicense.org
# ============================================================================*/

# Prerequisite : Options.cmake should run first
   
   SET(HEADER  "/** ==========================================================================
* 2015 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
* 
* For more information see g3log/LICENSE or refer refer to http://unlicense.org
* ============================================================================*/")



message( STATUS "" )
message( STATUS "COMPILE_DEFINITIONS:\n\t[${G3_DEFINITIONS}]" )
message( STATUS "" )

SET(GENERATED_G3_DEFINITIONS "${CMAKE_CURRENT_BINARY_DIR}/include/g3log/generated_definitions.hpp")

# If it exists, read existing file
set(current_content "")
if(EXISTS ${GENERATED_G3_DEFINITIONS})
  file(READ ${GENERATED_G3_DEFINITIONS} current_content)
endif()

set(generated_content "// AUTO GENERATED MACRO DEFINITIONS FOR G3LOG\n\n")
set(generated_content "${generated_content}\n${HEADER}\n")
set(generated_content "${generated_content}\n#pragma once\n\n")
set(generated_content "${generated_content}\n// CMake induced definitions below. See g3log/Options.cmake for details.\n\n")

FOREACH(definition ${G3_DEFINITIONS} )
  set(generated_content "${generated_content}\n#define ${definition}\n")
ENDFOREACH(definition)

if(NOT "${current_content}" STREQUAL "${generated_content}")

  message( STATUS "Generated ${GENERATED_G3_DEFINITIONS}" )

  message( STATUS "******************** START *************************" )
  message(${generated_content})
  message( STATUS "******************** END *************************" )

  file(WRITE ${GENERATED_G3_DEFINITIONS} ${generated_content})

endif()

