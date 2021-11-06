cmake_minimum_required( VERSION 2.4 )
if( COMMAND cmake_policy )
	cmake_policy( SET CMP0003 NEW )
endif( COMMAND cmake_policy )

include( CheckFunctionExists )
include( CheckCXXCompilerFlag )

project( ZDRay )

SET( CMAKE_CXX_STANDARD 14 )

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
	# Enable C++ conformance mode
	set( ALL_C_FLAGS "/GF /Gy /GR- /permissive-" )
	
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
	set( ALL_C_FLAGS "${ALL_C_FLAGS} -ffast-math -pipe -pthread" )
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
	src/framework/zstring.cpp
	src/framework/zstrformat.cpp
	src/framework/utf8.cpp
	src/framework/utf8.h
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
	src/lightmap/pngwriter.cpp
	src/lightmap/raytracer.cpp
	src/lightmap/surfaces.cpp
	src/lightmap/worker.cpp
	src/lightmap/collision.cpp
	src/lightmap/delauneytriangulator.cpp
	src/lightmap/delauneytriangulator.h
	src/lightmap/vulkandevice.cpp
	src/lightmap/vulkandevice.h
	src/lightmap/vulkanobjects.h
	src/lightmap/vulkanbuilders.cpp
	src/lightmap/vulkanbuilders.h
	src/lightmap/stacktrace.cpp
	src/lightmap/stacktrace.h
	src/lightmap/gpuraytracer.cpp
	src/lightmap/gpuraytracer.h
	src/lightmap/glsl_closesthit.h
	src/lightmap/glsl_miss.h
	src/lightmap/glsl_raygen.h
	src/math/angle.cpp
	src/math/bounds.cpp
	src/math/mathlib.cpp
	src/math/matrix.cpp
	src/math/plane.cpp
	src/math/quaternion.cpp
	src/math/vector.cpp
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
	src/lightmap/pngwriter.h
	src/lightmap/raytracer.h
	src/lightmap/surfaces.h
	src/lightmap/worker.h
	src/lightmap/collision.h
	src/math/mathlib.h
)

set(THIRDPARTY_SOURCES
	../thirdparty/vk_mem_alloc/vk_mem_alloc.h
	../thirdparty/vk_mem_alloc/vk_mem_alloc.cpp
	../thirdparty/volk/volk.c
	../thirdparty/volk/volk.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/SymbolTable.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/propagateNoContraction.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/PoolAlloc.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/Intermediate.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/gl_types.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/parseVersions.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/attribute.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/Scan.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/iomapper.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/ParseHelper.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/glslang_tab.cpp.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/SymbolTable.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/RemoveTree.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/Versions.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/reflection.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/LiveTraverser.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/iomapper.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/intermOut.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/Versions.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/Initialize.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/linkValidate.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/InfoSink.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/Constant.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/IntermTraverse.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/propagateNoContraction.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/glslang_tab.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/ShaderLang.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/preprocessor/Pp.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/preprocessor/PpTokens.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/preprocessor/PpAtom.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/preprocessor/PpContext.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/preprocessor/PpTokens.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/preprocessor/PpScanner.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/preprocessor/PpContext.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/attribute.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/localintermediate.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/parseConst.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/Initialize.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/limits.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/ParseContextBase.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/RemoveTree.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/ParseHelper.cpp
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/Scan.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/reflection.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/ScanContext.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/pch.h
	../thirdparty/ShaderCompiler/glslang/MachineIndependent/SpirvIntrinsics.cpp
	../thirdparty/ShaderCompiler/glslang/OSDependent/osinclude.h
	../thirdparty/ShaderCompiler/glslang/GenericCodeGen/Link.cpp
	../thirdparty/ShaderCompiler/glslang/GenericCodeGen/CodeGen.cpp
	../thirdparty/ShaderCompiler/glslang/Public/ShaderLang.h
	../thirdparty/ShaderCompiler/glslang/Include/ConstantUnion.h
	../thirdparty/ShaderCompiler/glslang/Include/InitializeGlobals.h
	../thirdparty/ShaderCompiler/glslang/Include/Common.h
	../thirdparty/ShaderCompiler/glslang/Include/PoolAlloc.h
	../thirdparty/ShaderCompiler/glslang/Include/arrays.h
	../thirdparty/ShaderCompiler/glslang/Include/ShHandle.h
	../thirdparty/ShaderCompiler/glslang/Include/InfoSink.h
	../thirdparty/ShaderCompiler/glslang/Include/ResourceLimits.h
	../thirdparty/ShaderCompiler/glslang/Include/Types.h
	../thirdparty/ShaderCompiler/glslang/Include/BaseTypes.h
	../thirdparty/ShaderCompiler/glslang/Include/intermediate.h
	../thirdparty/ShaderCompiler/glslang/Include/SpirvIntrinsics.h
	../thirdparty/ShaderCompiler/spirv/Logger.h
	../thirdparty/ShaderCompiler/spirv/GlslangToSpv.cpp
	../thirdparty/ShaderCompiler/spirv/SPVRemapper.h
	../thirdparty/ShaderCompiler/spirv/GLSL.ext.EXT.h
	../thirdparty/ShaderCompiler/spirv/hex_float.h
	../thirdparty/ShaderCompiler/spirv/doc.cpp
	../thirdparty/ShaderCompiler/spirv/disassemble.cpp
	../thirdparty/ShaderCompiler/spirv/SpvPostProcess.cpp
	../thirdparty/ShaderCompiler/spirv/bitutils.h
	../thirdparty/ShaderCompiler/spirv/InReadableOrder.cpp
	../thirdparty/ShaderCompiler/spirv/GLSL.ext.AMD.h
	../thirdparty/ShaderCompiler/spirv/GLSL.ext.NV.h
	../thirdparty/ShaderCompiler/spirv/SPVRemapper.cpp
	../thirdparty/ShaderCompiler/spirv/SpvBuilder.h
	../thirdparty/ShaderCompiler/spirv/GLSL.ext.KHR.h
	../thirdparty/ShaderCompiler/spirv/disassemble.h
	../thirdparty/ShaderCompiler/spirv/SpvBuilder.cpp
	../thirdparty/ShaderCompiler/spirv/GlslangToSpv.h
	../thirdparty/ShaderCompiler/spirv/doc.h
	../thirdparty/ShaderCompiler/spirv/SpvTools.cpp
	../thirdparty/ShaderCompiler/spirv/spvIR.h
	../thirdparty/ShaderCompiler/spirv/Logger.cpp
	../thirdparty/ShaderCompiler/spirv/SpvTools.h
	../thirdparty/ShaderCompiler/spirv/GLSL.std.450.h
	../thirdparty/ShaderCompiler/OGLCompilersDLL/InitializeDll.cpp
	../thirdparty/ShaderCompiler/OGLCompilersDLL/InitializeDll.h
	../thirdparty/vulkan/vk_sdk_platform.h
	../thirdparty/vulkan/vulkan_fuchsia.h
	../thirdparty/vulkan/vulkan_win32.h
	../thirdparty/vulkan/vulkan_ios.h
	../thirdparty/vulkan/vk_icd.h
	../thirdparty/vulkan/vk_layer.h
	../thirdparty/vulkan/vulkan_xcb.h
	../thirdparty/vulkan/vulkan_macos.h
	../thirdparty/vulkan/vk_layer_dispatch_table.h
	../thirdparty/vulkan/vulkan_ggp.h
	../thirdparty/vulkan/vulkan_core.h
	../thirdparty/vulkan/vulkan_wayland.h
	../thirdparty/vulkan/vulkan_metal.h
	../thirdparty/vulkan/vulkan_android.h
	../thirdparty/vulkan/vulkan_vi.h
	../thirdparty/vulkan/vulkan.h
	../thirdparty/vulkan/vk_platform.h
	../thirdparty/vulkan/vulkan_mir.h
	../thirdparty/vulkan/vulkan_xlib_xrandr.h
	../thirdparty/vulkan/vulkan_xlib.h
	../thirdparty/vulkan/vk_enum_string_helper.h
	../thirdparty/vulkan/vulkan_beta.h
	../thirdparty/vulkan/vulkan_directfb.h
	../thirdparty/vulkan/vulkan_screen.h
)

set(THIRDPARTY_WIN32_SOURCES
	../thirdparty/ShaderCompiler/glslang/OSDependent/Windows/ossource.cpp
)

set(THIRDPARTY_UNIX_SOURCES
	../thirdparty/ShaderCompiler/glslang/OSDependent/Unix/ossource.cpp
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

	set(THIRDPARTY_SOURCES ${THIRDPARTY_SOURCES} ${THIRDPARTY_WIN32_SOURCES})
else()
	set(THIRDPARTY_SOURCES ${THIRDPARTY_SOURCES} ${THIRDPARTY_UNIX_SOURCES})
endif( WIN32 )

if(MSVC)
	# Use all cores for compilation
	set(CMAKE_CXX_FLAGS "/MP ${CMAKE_CXX_FLAGS}")

	# Ignore warnings in third party code
	set_source_files_properties(${THIRDPARTY_SOURCES} PROPERTIES COMPILE_FLAGS "/wd4244 /wd4267 /wd4005 /wd4018 -D_CRT_SECURE_NO_WARNINGS")
endif()

if( CMAKE_COMPILER_IS_GNUCXX )
	set( ZDRAY_LIBS "${ZLIB_LIBRARIES}" pthread )
endif( CMAKE_COMPILER_IS_GNUCXX )

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

add_executable( zdray ${SOURCES} ${HEADERS} ${THIRDPARTY_SOURCES} )
target_link_libraries( zdray ${ZDRAY_LIBS} ${PROF_LIB} )
include_directories( src "${ZLIB_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty" )

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
source_group("Sources\\Models" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/src/models/.+")

source_group("thirdparty" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/.+")
source_group("thirdparty\\ShaderCompiler" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ShaderCompiler/.+")
source_group("thirdparty\\vk_mem_alloc" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/vk_mem_alloc/.+")
source_group("thirdparty\\volk" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/volk/.+")
source_group("thirdparty\\vulkan" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/vulkan/.+")
