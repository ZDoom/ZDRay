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

set( ZDRAY_LIBS "" )
set( SOURCES
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
	src/lightmap/vulkandevice.cpp
	src/lightmap/vulkandevice.h
	src/lightmap/vulkanobjects.h
	src/lightmap/vulkanbuilders.cpp
	src/lightmap/vulkanbuilders.h
	src/lightmap/stacktrace.cpp
	src/lightmap/stacktrace.h
	src/lightmap/gpuraytracer.cpp
	src/lightmap/gpuraytracer.h
	src/lightmap/gpuraytracer2.cpp
	src/lightmap/gpuraytracer2.h
	src/lightmap/glsl_rchit_bounce.h
	src/lightmap/glsl_rchit_light.h
	src/lightmap/glsl_rchit_sun.h
	src/lightmap/glsl_rchit_ambient.h
	src/lightmap/glsl_rgen_bounce.h
	src/lightmap/glsl_rgen_light.h
	src/lightmap/glsl_rgen_ambient.h
	src/lightmap/glsl_rmiss_bounce.h
	src/lightmap/glsl_rmiss_light.h
	src/lightmap/glsl_rmiss_sun.h
	src/lightmap/glsl_rmiss_ambient.h
	src/lightmap/glsl_frag.h
	src/lightmap/glsl_vert.h
	src/lightmap/cpuraytracer.cpp
	src/lightmap/cpuraytracer.h
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
	${CMAKE_SOURCE_DIR}/thirdparty/miniz/miniz.h
	${CMAKE_SOURCE_DIR}/thirdparty/miniz/miniz.c
	${CMAKE_SOURCE_DIR}/thirdparty/vk_mem_alloc/vk_mem_alloc.h
	${CMAKE_SOURCE_DIR}/thirdparty/vk_mem_alloc/vk_mem_alloc.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/volk/volk.c
	${CMAKE_SOURCE_DIR}/thirdparty/volk/volk.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/SymbolTable.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/propagateNoContraction.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/PoolAlloc.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/Intermediate.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/gl_types.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/parseVersions.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/attribute.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/Scan.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/iomapper.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/ParseHelper.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/glslang_tab.cpp.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/SymbolTable.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/RemoveTree.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/Versions.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/reflection.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/LiveTraverser.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/iomapper.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/intermOut.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/Versions.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/Initialize.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/linkValidate.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/InfoSink.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/Constant.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/IntermTraverse.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/propagateNoContraction.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/glslang_tab.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/ShaderLang.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/preprocessor/Pp.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/preprocessor/PpTokens.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/preprocessor/PpAtom.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/preprocessor/PpContext.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/preprocessor/PpTokens.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/preprocessor/PpScanner.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/preprocessor/PpContext.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/attribute.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/localintermediate.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/parseConst.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/Initialize.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/limits.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/ParseContextBase.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/RemoveTree.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/ParseHelper.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/Scan.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/reflection.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/ScanContext.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/pch.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/MachineIndependent/SpirvIntrinsics.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/OSDependent/osinclude.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/GenericCodeGen/Link.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/GenericCodeGen/CodeGen.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/Public/ShaderLang.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/Include/ConstantUnion.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/Include/InitializeGlobals.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/Include/Common.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/Include/PoolAlloc.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/Include/arrays.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/Include/ShHandle.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/Include/InfoSink.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/Include/ResourceLimits.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/Include/Types.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/Include/BaseTypes.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/Include/intermediate.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/Include/SpirvIntrinsics.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/Logger.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/GlslangToSpv.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/SPVRemapper.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/GLSL.ext.EXT.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/hex_float.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/doc.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/disassemble.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/SpvPostProcess.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/bitutils.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/InReadableOrder.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/GLSL.ext.AMD.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/GLSL.ext.NV.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/SPVRemapper.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/SpvBuilder.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/GLSL.ext.KHR.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/disassemble.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/SpvBuilder.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/GlslangToSpv.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/doc.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/SpvTools.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/spvIR.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/Logger.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/SpvTools.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/spirv/GLSL.std.450.h
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/OGLCompilersDLL/InitializeDll.cpp
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/OGLCompilersDLL/InitializeDll.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vk_sdk_platform.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan_fuchsia.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan_win32.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan_ios.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vk_icd.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vk_layer.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan_xcb.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan_macos.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vk_layer_dispatch_table.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan_ggp.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan_core.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan_wayland.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan_metal.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan_android.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan_vi.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vk_platform.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan_mir.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan_xlib_xrandr.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan_xlib.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vk_enum_string_helper.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan_beta.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan_directfb.h
	${CMAKE_SOURCE_DIR}/thirdparty/vulkan/vulkan_screen.h
)

set(THIRDPARTY_WIN32_SOURCES
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/OSDependent/Windows/ossource.cpp
)

set(THIRDPARTY_UNIX_SOURCES
	${CMAKE_SOURCE_DIR}/thirdparty/ShaderCompiler/glslang/OSDependent/Unix/ossource.cpp
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

if (WIN32)
else()
set(PLATFORM_LIB "-ldl -lpthread")
endif()

if( CMAKE_COMPILER_IS_GNUCXX )
	set( ZDRAY_LIBS pthread )
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

add_executable( zdray ${SOURCES} ${THIRDPARTY_SOURCES} )
target_link_libraries( zdray ${ZDRAY_LIBS} ${PROF_LIB} ${PLATFORM_LIB} )
include_directories( src "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty" )

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
source_group("thirdparty\\miniz" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/miniz/.+")
