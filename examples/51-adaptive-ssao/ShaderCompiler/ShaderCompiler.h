#pragma once

#include <bx/allocator.h>
#include <bx/debug.h>
#include <bx/file.h>
#include <bx/math.h>
#include <bx/rng.h>

#include "bgfx_utils.h"
#include <bgfx/bgfx.h>
#include <bx/file.h>

#include <cstdio>
#include <iostream>
#include <vector>
#include <random>

namespace Dolphin
{
	typedef void(*UserErrorFn)(void*, const char*, va_list);
	static UserErrorFn s_user_error_fn = nullptr;
	static void* s_user_error_ptr = nullptr;

	#define getUniformTypeName getUniformTypeName_shaderc
	#define nameToUniformTypeEnum nameToUniformTypeEnum_shaderc
	#define s_uniformTypeName s_uniformTypeName_shaderc

	void setShaderCErrorFunction(UserErrorFn fn, void* user_ptr);

	void printError(FILE* file, const char* format, ...);

	bgfx::ProgramHandle compileGraphicsShader(const char* vsPath, const char* fsPath, const char* defPath);
	bgfx::ProgramHandle compileComputeShader(const char* csPath);
}
