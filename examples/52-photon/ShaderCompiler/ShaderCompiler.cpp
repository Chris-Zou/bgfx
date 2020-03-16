#include "ShaderCompiler.h"

namespace Dolphin
{
	void printError(FILE* file, const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		if (s_user_error_fn)
		{
			s_user_error_fn(s_user_error_ptr, format, args);
		}
		else
		{
			vfprintf(file, format, args);
		}
		va_end(args);
	}

	void setShaderCErrorFunction(UserErrorFn fn, void* user_ptr)
	{
		s_user_error_fn = fn;
		s_user_error_ptr = user_ptr;
	}

	bgfx::ProgramHandle compileGraphicsShader(const char* vsPath, const char* fsPath, const char* defPath)
	{
		if (vsPath == nullptr || fsPath == nullptr || defPath == nullptr)
			return BGFX_INVALID_HANDLE;

		const bgfx::Memory* memVsh = shaderc::compileShader(shaderc::ST_VERTEX, vsPath, "", defPath);
		if (memVsh == nullptr)
			return BGFX_INVALID_HANDLE;

		const bgfx::Memory* memFsh = shaderc::compileShader(shaderc::ST_FRAGMENT, fsPath, "", defPath);
		if (memFsh == nullptr)
			return BGFX_INVALID_HANDLE;

		bgfx::ShaderHandle vsh = bgfx::createShader(memVsh);
		bgfx::ShaderHandle fsh = bgfx::createShader(memFsh);

		return bgfx::createProgram(vsh, fsh, true);
	}

	bgfx::ProgramHandle compileComputeShader(const char* csPath)
	{
		if (csPath == nullptr)
			return BGFX_INVALID_HANDLE;

		const bgfx::Memory* memCs = shaderc::compileShader(shaderc::ST_COMPUTE, csPath);
		if (memCs != nullptr)
		{
			bgfx::ShaderHandle cSh = bgfx::createShader(memCs);
			return bgfx::createProgram(cSh, true);
		}

		return BGFX_INVALID_HANDLE;
	}
}
