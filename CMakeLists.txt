cmake_minimum_required( VERSION 2.4 )
if( COMMAND cmake_policy )
	cmake_policy( SET CMP0003 NEW )
endif( COMMAND cmake_policy )

include( CheckFunctionExists )
include( CheckCXXCompilerFlag )

project( ZDRay )

IF( NOT CMAKE_BUILD_TYPE )
	SET( CMAKE_BUILD_TYPE Debug CACHE STRING
		"Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
		FORCE )
ENDIF( NOT CMAKE_BUILD_TYPE )

set( SSE_MATTERS NO )

# SSE only matters on 32-bit targets. We check compiler flags to know if we can do it.
if( CMAKE_SIZEOF_VOID_P MATCHES "4" )
	CHECK_CXX_COMPILER_FLAG( "-msse2 -mfpmath=sse" CAN_DO_MFPMATH )
	CHECK_CXX_COMPILER_FLAG( -arch:SSE2 CAN_DO_ARCHSSE2 )
	if( CAN_DO_MFPMATH )
		set( SSE1_ENABLE "-msse -mfpmath=sse" )
		set( SSE2_ENABLE "-msse2 -mfpmath=sse" )
		set( SSE_MATTERS YES )
	elseif( CAN_DO_ARCHSSE2 )
		set( SSE1_ENABLE -arch:SSE )
		set( SSE2_ENABLE -arch:SSE2 )
		set( SSE_MATTERS YES )
	endif( CAN_DO_MFPMATH )
endif( CMAKE_SIZEOF_VOID_P MATCHES "4" )

if( SSE_MATTERS )
	if( WIN32 )
		set( BACKPATCH 1 CACHE BOOL "Enable backpatching." )
	else( WIN32 )
		CHECK_FUNCTION_EXISTS(mprotect HAVE_MPROTECT)
		if( HAVE_MPROTECT )
			set( BACKPATCH 1 CACHE BOOL "Enable backpatching." )
		else( HAVE_MPROTECT )
			set( BACKPATCH 0 )
		endif( HAVE_MPROTECT )
	endif( WIN32 )
	set( FULL_SSE2 0 CACHE BOOL "Use SSE2 math everywhere." )
	set( SSE 1 CACHE BOOL "Build SSE and SSE2 versions of key code." )
else( SSE_MATTERS )
	set( BACKPATCH 0 )
endif( SSE_MATTERS )

if( CMAKE_COMPILER_IS_GNUCXX )
	set( GPROF 0 CACHE BOOL "Enable profiling with gprof for Debug and RelWithDebInfo build types." )
	set( PROFILE 0 CACHE INT "1=Generate profile coverage info, 2=Use it" )
endif( CMAKE_COMPILER_IS_GNUCXX )

find_package( ZLIB )

if( MSVC )
	# Eliminate unreferenced functions and data
	# Perform identical COMDAT folding
	set( REL_LINKER_FLAGS "/opt:ref /opt:icf /nodefaultlib:msvcrt" )

	# String pooling
	# Function-level linking
	# Disable run-time type information
	set( ALL_C_FLAGS "/GF /Gy /GR-" )
	
	# Avoid CRT DLL dependancies in release builds
	set( REL_C_FLAGS "/MT" )

	# Disable warnings for unsecure CRT functions from VC8+
	if( MSVC_VERSION GREATER 1399 )
		set( ALL_C_FLAGS "${ALL_C_FLAGS} /wd4996" )
	endif( MSVC_VERSION GREATER 1399 )
	
	# The CMake configurations set /GR and /MD by default, which conflict with our settings.
	string(REPLACE "/MD " " " CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE} )
	string(REPLACE "/MD " " " CMAKE_CXX_FLAGS_MINSIZEREL ${CMAKE_CXX_FLAGS_MINSIZEREL} )
	string(REPLACE "/MD " " " CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} )
	string(REPLACE "/MD " " " CMAKE_C_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE} )
	string(REPLACE "/MD " " " CMAKE_C_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS_MINSIZEREL} )
	string(REPLACE "/MD " " " CMAKE_C_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS_RELWITHDEBINFO} )
	string(REPLACE " /GR" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} )
endif( MSVC )

if( CMAKE_COMPILER_IS_GNUCXX )
	set( ALL_C_FLAGS "${ALL_C_FLAGS} -ffast-math -pipe" )
	if( GPROF )
		set( ALL_C_FLAGS "${ALL_C_FLAGS} -pg -g" )
	else( GPROF )
		set( REL_C_FLAGS "${REL_C_FLAGS} -fomit-frame-pointer" )
	endif( GPROF )
	if( PROFILE EQUAL 1 )
		message( STATUS "Generating profile coverage information" )
		set( ALL_C_FLAGS "${ALL_C_FLAGS} -fprofile-generate" )
		set( PROF_LIB "gcov" )
	elseif( PROFILE EQUAL 2 )
		message( STATUS "Using profile coverage information" )
		set( ALL_C_FLAGS "${ALL_C_FLAGS} -fprofile-use" )
	endif( PROFILE EQUAL 1 )
endif( CMAKE_COMPILER_IS_GNUCXX )

#if( WIN32 AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" )
#	set( ALL_C_FLAGS "${ALL_C_FLAGS} -Wno-deprecated-declarations -Wno-format" )
#endif( WIN32 AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" )

if( ZLIB_FOUND )
	message( STATUS "Using system zlib" )
else( ZLIB_FOUND )
	message( STATUS "Using internal zlib" )
	add_subdirectory( zlib )
	set( ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/zlib )
	set( ZLIB_LIBRARIES z )
	set( ZLIB_LIBRARY z )
endif( ZLIB_FOUND )

if( BACKPATCH )
	add_definitions( -DBACKPATCH )
endif( BACKPATCH )

CHECK_FUNCTION_EXISTS( stricmp STRICMP_EXISTS )
if( NOT STRICMP_EXISTS )
	add_definitions( -Dstricmp=strcasecmp )
endif( NOT STRICMP_EXISTS )

CHECK_FUNCTION_EXISTS( strnicmp STRNICMP_EXISTS )
if( NOT STRNICMP_EXISTS )
	add_definitions( -Dstrnicmp=strncasecmp )
endif( NOT STRNICMP_EXISTS )

set( ZDRAY_LIBS "${ZLIB_LIBRARIES}" )
set( SOURCES
	src/main.cpp
	src/commandline/getopt.c
	src/commandline/getopt1.c
	src/framework/halffloat.cpp
	src/framework/binfile.cpp
	src/blockmapbuilder/blockmapbuilder.cpp
	src/level/level.cpp
	src/level/level_udmf.cpp
	src/level/level_light.cpp
	src/parse/sc_man.cpp
	src/wad/wad.cpp
	src/nodebuilder/nodebuild.cpp
	src/nodebuilder/nodebuild_events.cpp
	src/nodebuilder/nodebuild_extract.cpp
	src/nodebuilder/nodebuild_gl.cpp
	src/nodebuilder/nodebuild_utility.cpp
	src/nodebuilder/nodebuild_classify_nosse2.cpp
	src/lightmap/lightmap.cpp
	src/lightmap/lightsurface.cpp
	src/lightmap/surfaces.cpp
	src/lightmap/worker.cpp
	src/lightmap/collision.cpp
	src/math/angle.cpp
	src/math/bounds.cpp
	src/math/mathlib.cpp
	src/math/matrix.cpp
	src/math/plane.cpp
	src/math/pluecker.cpp
	src/math/quaternion.cpp
	src/math/random.cpp
	src/math/vector.cpp
)

set( HEADERS
	src/commandline/getopt.h
	src/blockmapbuilder/blockmapbuilder.h
	src/nodebuilder/nodebuild.h
	src/level/doomdata.h
	src/level/level.h
	src/level/workdata.h
	src/parse/sc_man.h
	src/wad/wad.h
	src/platform/windows/resource.h
	src/framework/tarray.h
	src/framework/templates.h
	src/framework/zdray.h
	src/framework/xs_Float.h
	src/framework/halffloat.h
	src/framework/binfile.h
	src/lightmap/lightmap.h
	src/lightmap/lightsurface.h
	src/lightmap/surfaces.h
	src/lightmap/worker.h
	src/lightmap/collision.h
	src/math/mathlib.h
)

if( SSE_MATTERS )
	if( FULL_SSE2 )
		message( STATUS "Using SSE2 math everywhere." )
		# Building everything with SSE2 is much like disabling it, in that we
		# need not check for its existance while running.
		set( ALL_C_FLAGS "${ALL_C_FLAGS} -DDISABLE_SSE ${SSE2_ENABLE}" )
	else( FULL_SSE2 )
	if( SSE )
		message( STATUS "Using SSE math for ClassifyLine only." )
		set( SOURCES ${SOURCES} src/nodebuilder/nodebuild_classify_sse1.cpp src/nodebuilder/nodebuild_classify_sse2.cpp )
		set_source_files_properties( src/nodebuilder/nodebuild_classify_sse1.cpp PROPERTIES COMPILE_FLAGS "${SSE1_ENABLE}" )
		set_source_files_properties( src/nodebuilder/nodebuild_classify_sse2.cpp PROPERTIES COMPILE_FLAGS "${SSE2_ENABLE}" )
	else( SSE )
		message( STATUS "SSE math is completely disabled." )
		set( ALL_C_FLAGS "${ALL_C_FLAGS} -DDISABLE_SSE" )
	endif( SSE )
	endif( FULL_SSE2 )
else( SSE_MATTERS )
	set( ALL_C_FLAGS "${ALL_C_FLAGS} -DDISABLE_SSE" )
endif( SSE_MATTERS )

if( WIN32 )
	set( ZDRAY_LIBS ${ZDRAY_LIBS} user32 gdi32 )

	if( CMAKE_COMPILER_IS_GNUCXX )
		# CMake is not set up to compile and link rc files with GCC. :(
		add_custom_command( OUTPUT zdray-rc.o
			COMMAND windres -o zdray-rc.o -i ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows/resource.rc
			DEPENDS resource.rc )
		set( SOURCES ${SOURCES} zdray-rc.o )
	else( CMAKE_COMPILER_IS_GNUCXX )
		set( SOURCES ${SOURCES} src/platform/windows/resource.rc )
	endif( CMAKE_COMPILER_IS_GNUCXX )
endif( WIN32 )


set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${REL_LINKER_FLAGS}" )
set( CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} ${REL_LINKER_FLAGS}" )
set( CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} ${REL_LINKER_FLAGS}" )

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ALL_C_FLAGS}" )
set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${REL_C_FLAGS}" )
set( CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} ${REL_C_FLAGS}" )
set( CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${REL_C_FLAGS}" )
set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${DEB_C_FLAGS} -D_DEBUG" )

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ALL_C_FLAGS}" )
set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${REL_C_FLAGS}" )
set( CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} ${REL_C_FLAGS}" )
set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${REL_C_FLAGS}" )
set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${DEB_C_FLAGS} -D_DEBUG" )

add_executable( zdray ${SOURCES} ${HEADERS} )
target_link_libraries( zdray ${ZDRAY_LIBS} ${PROF_LIB} )
include_directories( src "${ZLIB_INCLUDE_DIR}" )

source_group("Sources" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/.+")
source_group("Sources\\BlockmapBuilder" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/blockmapbuilder/.+")
source_group("Sources\\Commandline" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/commandline/.+")
source_group("Sources\\Framework" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/framework/.+")
source_group("Sources\\Level" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/level/.+")
source_group("Sources\\NodeBuilder" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/nodebuilder/.+")
source_group("Sources\\Parse" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/parse/.+")
source_group("Sources\\Platform" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/platform/.+")
source_group("Sources\\Platform\\Windows" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows/.+")
source_group("Sources\\Wad" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/wad/.+")
source_group("Sources\\Math" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/math/.+")
source_group("Sources\\Lightmap" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/lightmap/.+")
