cmake_minimum_required(VERSION 3.15)

add_subdirectory( thirdparty/ZVulkan )

project(zdray)

include(CheckFunctionExists)

set(ZDRAY_SOURCES
	src/main.cpp
	src/commandline/getopt.c
	src/commandline/getopt1.c
	src/commandline/getopt.h
	src/framework/halffloat.cpp
	src/framework/binfile.cpp
	src/framework/zstring.cpp
	src/framework/zstrformat.cpp
	src/framework/utf8.cpp
	src/framework/utf8.h
	src/framework/tarray.h
	src/framework/templates.h
	src/framework/zdray.h
	src/framework/xs_Float.h
	src/framework/halffloat.h
	src/framework/binfile.h
	src/blockmapbuilder/blockmapbuilder.cpp
	src/blockmapbuilder/blockmapbuilder.h
	src/level/level.cpp
	src/level/level_udmf.cpp
	src/level/level_light.cpp
	src/level/level_slopes.cpp
	src/level/doomdata.h
	src/level/level.h
	src/level/workdata.h
	src/parse/sc_man.cpp
	src/parse/sc_man.h
	src/wad/wad.cpp
	src/wad/wad.h
	src/nodebuilder/nodebuild.cpp
	src/nodebuilder/nodebuild_events.cpp
	src/nodebuilder/nodebuild_extract.cpp
	src/nodebuilder/nodebuild_gl.cpp
	src/nodebuilder/nodebuild_utility.cpp
	src/nodebuilder/nodebuild_classify_nosse2.cpp
	src/nodebuilder/nodebuild.h
	src/lightmap/pngwriter.cpp
	src/lightmap/pngwriter.h
	src/lightmap/levelmesh.cpp
	src/lightmap/levelmesh.h
	src/lightmap/lightmaptexture.cpp
	src/lightmap/lightmaptexture.h
	src/lightmap/collision.cpp
	src/lightmap/collision.h
	src/lightmap/delauneytriangulator.cpp
	src/lightmap/delauneytriangulator.h
	src/lightmap/stacktrace.cpp
	src/lightmap/stacktrace.h
	src/lightmap/surfaceclip.cpp
	src/lightmap/surfaceclip.h
	src/lightmap/gpuraytracer.cpp
	src/lightmap/gpuraytracer.h
	src/lightmap/glsl_frag.h
	src/lightmap/glsl_vert.h
	src/lightmap/glsl_frag_resolve.h
	src/lightmap/renderdoc_app.h
	src/lightmap/portal.h
	src/math/mat.cpp
	src/math/plane.cpp
	src/math/angle.cpp
	src/math/bounds.cpp
	src/math/mathlib.cpp
	src/math/mat.h
	src/math/quaternion.h
	src/math/vec.h
	src/math/mathlib.h
	src/models/model.cpp
	src/models/model.h
	src/models/model_md2.h
	src/models/model_md3.h
	src/models/model_obj.h
	src/models/model_ue1.h
	src/models/modelrenderer.h
	src/models/models_md2.cpp
	src/models/models_md3.cpp
	src/models/models_obj.cpp
	src/models/models_ue1.cpp
	src/models/tab_anorms.h
	src/platform/windows/resource.h
)

set(THIRDPARTY_SOURCES
	${CMAKE_SOURCE_DIR}/thirdparty/dp_rect_pack/dp_rect_pack.h
	${CMAKE_SOURCE_DIR}/thirdparty/miniz/miniz.h
	${CMAKE_SOURCE_DIR}/thirdparty/miniz/miniz.c
)

set(ZDRAY_LIBS
	zvulkan
)

source_group("src" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/.+")
source_group("src\\BlockmapBuilder" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/blockmapbuilder/.+")
source_group("src\\Commandline" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/commandline/.+")
source_group("src\\Framework" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/framework/.+")
source_group("src\\Level" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/level/.+")
source_group("src\\NodeBuilder" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/nodebuilder/.+")
source_group("src\\Parse" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/parse/.+")
source_group("src\\Platform" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/platform/.+")
source_group("src\\Platform\\Windows" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows/.+")
source_group("src\\Wad" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/wad/.+")
source_group("src\\Math" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/math/.+")
source_group("src\\Lightmap" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/lightmap/.+")
source_group("src\\Models" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/models/.+")

source_group("thirdparty" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/.+")
source_group("thirdparty\\dp_rect_pack" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/dp_rect_pack/.+")
source_group("thirdparty\\miniz" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/miniz/.+")

include_directories( src "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ZVulkan/include" "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty" )

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}")
include(TargetArch)
target_architecture(TARGET_ARCHITECTURE)
if(${TARGET_ARCHITECTURE} MATCHES "x86_64")
	set(ZDRAY_SOURCES ${ZDRAY_SOURCES}
		src/nodebuilder/nodebuild_classify_sse1.cpp
		src/nodebuilder/nodebuild_classify_sse2.cpp
	)
	set_source_files_properties(src/nodebuilder/nodebuild_classify_sse1.cpp PROPERTIES COMPILE_FLAGS "${SSE1_ENABLE}")
	set_source_files_properties(src/nodebuilder/nodebuild_classify_sse2.cpp PROPERTIES COMPILE_FLAGS "${SSE2_ENABLE}")
else()
	add_definitions(-DNO_SSE)
endif()

CHECK_FUNCTION_EXISTS(stricmp STRICMP_EXISTS)
if(NOT STRICMP_EXISTS)
	add_definitions(-Dstricmp=strcasecmp)
endif()

CHECK_FUNCTION_EXISTS( strnicmp STRNICMP_EXISTS )
if(NOT STRNICMP_EXISTS)
	add_definitions(-Dstrnicmp=strncasecmp)
endif()

if(WIN32)
	add_definitions(-DUNICODE -D_UNICODE)
else()
	add_definitions(-DUNIX -D_UNIX)
	add_link_options(-pthread)
endif()

if(MSVC)
	# Use all cores for compilation
	set(CMAKE_CXX_FLAGS "/MP ${CMAKE_CXX_FLAGS}")

	# zdbsp source code is using some ancient C code that really should be updated
	set_source_files_properties(${ZDRAY_SOURCES} PROPERTIES COMPILE_FLAGS "/wd4996 -D_CRT_SECURE_NO_WARNINGS")

	# Ignore warnings in third party code
	#set_source_files_properties(${THIRDPARTY_SOURCES} PROPERTIES COMPILE_FLAGS "/wd4244 /wd4267 /wd4005 /wd4018 -D_CRT_SECURE_NO_WARNINGS")
endif()

add_executable(zdray ${ZDRAY_SOURCES} ${THIRDPARTY_SOURCES})
target_link_libraries(zdray ${ZDRAY_LIBS})
set_target_properties(zdray PROPERTIES CXX_STANDARD 17)

if(MSVC)
	set_property(TARGET zdray PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
