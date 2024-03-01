cmake_minimum_required(VERSION 3.15)

add_subdirectory( thirdparty/ZVulkan )

project(zdray)

include(CheckFunctionExists)

set(ZDRAY_SOURCES
	src/main.cpp
	src/commandline/getopt.c
	src/commandline/getopt1.c
	src/commandline/getopt.h
	src/framework/binfile.cpp
	src/framework/binfile.h
	src/framework/halffloat.cpp
	src/framework/zstring.cpp
	src/framework/zstrformat.cpp
	src/framework/utf8.cpp
	src/framework/utf8.h
	src/framework/tarray.h
	src/framework/templates.h
	src/framework/zdray.h
	src/framework/xs_Float.h
	src/framework/halffloat.h
	src/framework/vectors.h
	src/framework/matrix.cpp
	src/framework/matrix.h
	src/framework/bounds.cpp
	src/framework/bounds.h
	src/framework/textureid.cpp
	src/framework/textureid.h
	src/framework/zipreader.cpp
	src/framework/zipreader.h
	src/framework/file.cpp
	src/framework/file.h
	src/framework/utf16.cpp
	src/framework/utf16.h
	src/framework/filesystem.cpp
	src/framework/filesystem.h
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
	src/lightmapper/hw_levelmesh.cpp
	src/lightmapper/hw_levelmesh.h
	src/lightmapper/hw_levelmeshlight.h
	src/lightmapper/hw_levelmeshportal.h
	src/lightmapper/hw_levelmeshsurface.h
	src/lightmapper/hw_lightmaptile.h
	src/lightmapper/flatvertices.h
	src/lightmapper/hw_materialstate.h
	src/lightmapper/hw_surfaceuniforms.h
	src/lightmapper/hw_collision.cpp
	src/lightmapper/hw_collision.h
	src/lightmapper/vk_renderdevice.cpp
	src/lightmapper/vk_renderdevice.h
	src/lightmapper/vk_levelmesh.cpp
	src/lightmapper/vk_levelmesh.h
	src/lightmapper/vk_lightmapper.cpp
	src/lightmapper/vk_lightmapper.h
	src/lightmapper/doom_levelmesh.cpp
	src/lightmapper/doom_levelmesh.h
	src/lightmapper/gpuraytracer.cpp
	src/lightmapper/gpuraytracer.h
	src/lightmapper/stacktrace.cpp
	src/lightmapper/stacktrace.h
	src/lightmapper/glsl/binding_lightmapper.glsl.h
	src/lightmapper/glsl/binding_raytrace.glsl.h
	src/lightmapper/glsl/binding_textures.glsl.h
	src/lightmapper/glsl/frag_blur.glsl.h
	src/lightmapper/glsl/frag_copy.glsl.h
	src/lightmapper/glsl/frag_raytrace.glsl.h
	src/lightmapper/glsl/frag_resolve.glsl.h
	src/lightmapper/glsl/montecarlo.glsl.h
	src/lightmapper/glsl/polyfill_rayquery.glsl.h
	src/lightmapper/glsl/trace_ambient_occlusion.glsl.h
	src/lightmapper/glsl/trace_bounce.glsl.h
	src/lightmapper/glsl/trace_levelmesh.glsl.h
	src/lightmapper/glsl/trace_light.glsl.h
	src/lightmapper/glsl/trace_sunlight.glsl.h
	src/lightmapper/glsl/vert_copy.glsl.h
	src/lightmapper/glsl/vert_raytrace.glsl.h
	src/lightmapper/glsl/vert_screenquad.glsl.h
	src/models/model.cpp
	src/models/model.h
	src/models/model_md2.h
	src/models/model_md3.h
	src/models/model_obj.h
	src/models/modelrenderer.h
	src/models/models_md2.cpp
	src/models/models_md3.cpp
	src/models/models_obj.cpp
	src/models/tab_anorms.h
	src/platform/windows/resource.h
)

set(THIRDPARTY_SOURCES
	${CMAKE_SOURCE_DIR}/thirdparty/dp_rect_pack/dp_rect_pack.h
	${CMAKE_SOURCE_DIR}/thirdparty/miniz/miniz.h
	${CMAKE_SOURCE_DIR}/thirdparty/miniz/miniz.c
	${CMAKE_SOURCE_DIR}/thirdparty/picopng/picopng.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/picopng/picopng.h
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
source_group("src\\Lightmapper" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/lightmapper/.+")
source_group("src\\Lightmapper\\glsl" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/lightmapper/glsl/.+")
source_group("src\\Models" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/models/.+")

source_group("thirdparty" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/.+")
source_group("thirdparty\\dp_rect_pack" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/dp_rect_pack/.+")
source_group("thirdparty\\miniz" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/miniz/.+")
source_group("thirdparty\\picopng" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/picopng/.+")

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
	add_definitions(-DDISABLE_SSE)
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
