# ----------------------------------------------------------------------------------
#	This file is part of the WhatsUpDoc tool
#	Copyright (C) 2018-2019 Sascha Brandt <sascha@brandt.graphics>
#	
#	This library is subject to the terms of the Mozilla Public License, v. 2.0.
#	You should have received a copy of the MPL along with this library; see the 
#	file LICENSE. If not, you can obtain one at http://mozilla.org/MPL/2.0/.
# ----------------------------------------------------------------------------------

# required by CMake
cmake_minimum_required(VERSION 3.1.0)

project(WhatsUpDoc)

# ensures that the project is build with C++11 standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add C++ source files to the project
add_executable(${PROJECT_NAME}
	src/Helper.cpp
	src/Parser.cpp
	src/WhatsUpDoc.cpp
)

# make sure that the built .dll or .so file is placed into the 'build' or 'bin' directory
#set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# --- Set up dependencies to the PADrend libraries ---

# add ./cmake to module search path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# find EScript library
find_package(EScript)
if(ESCRIPT_FOUND)
	target_include_directories(${PROJECT_NAME} PUBLIC ${ESCRIPT_INCLUDE_DIRS})
	target_link_libraries(${PROJECT_NAME} LINK_PUBLIC ${ESCRIPT_LIBRARIES})
endif()

find_package(LibClang REQUIRED)
if(LIBCLANG_FOUND)
	target_include_directories(${PROJECT_NAME} PUBLIC ${LIBCLANG_INCLUDE_DIRS})
	target_link_libraries(${PROJECT_NAME} LINK_PUBLIC ${LIBCLANG_LIBRARIES})
endif()

#find_package(LLVM REQUIRED)
#if(LLVM_FOUND)
#	target_include_directories(${PROJECT_NAME} PUBLIC ${LLVM_INCLUDE_DIRS})
#	target_link_libraries(${PROJECT_NAME} LINK_PUBLIC ${LLVM_LIBS_CORE} ${LLVM_LDFLAGS})
#endif()
#
#find_package(Clang REQUIRED)
#if(CLANG_FOUND)
#	target_include_directories(${PROJECT_NAME} PUBLIC ${CLANG_INCLUDE_DIRS})
#	target_link_libraries(${PROJECT_NAME} LINK_PUBLIC ${CLANG_LIBS})
#endif()
