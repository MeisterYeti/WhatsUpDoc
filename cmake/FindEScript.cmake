# Try to find EScript. Once done, this will define:
#
#   ESCRIPT_FOUND - variable which returns the result of the search
#   ESCRIPT_INCLUDE_DIRS - list of include directories
#   ESCRIPT_LIBRARIES - options for the linker

#=============================================================================
#
# This work is licensed under the Creative Commons Attribution-ShareAlike 4.0 International License.
# To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/4.0/.
# Author: Sascha Brandt (sascha@brandt.graphics)
#
#=============================================================================


# Find EScript include dir
find_path(ESCRIPT_INCLUDE_DIR
	EScript/EScript.h
	PATHS
	${CMAKE_CURRENT_SOURCE_DIR}/../EScript
)

# Find EScript
find_library(ESCRIPT_LIBRARY
	libEScript.so EScript
	PATHS
	${CMAKE_CURRENT_SOURCE_DIR}/../EScript
	${CMAKE_CURRENT_SOURCE_DIR}/../EScript/build
)

set(ESCRIPT_INCLUDE_DIRS ${ESCRIPT_INCLUDE_DIR})
set(ESCRIPT_LIBRARIES ${ESCRIPT_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(EScript DEFAULT_MSG
	ESCRIPT_INCLUDE_DIR
	ESCRIPT_LIBRARY
)

mark_as_advanced(
	ESCRIPT_INCLUDE_DIR
	ESCRIPT_LIBRARY
)
