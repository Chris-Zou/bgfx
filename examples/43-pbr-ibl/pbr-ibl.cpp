/*
 * Copyright 2019 Daniel Gavin. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

 /*
  * Reference(s):
  *
  * - Adaptive GPU Tessellation with Compute Shaders by Jad Khoury, Jonathan Dupuy, and Christophe Riccio
  *   http://onrendering.com/data/papers/isubd/isubd.pdf
  *   https://github.com/jdupuy/opengl-framework/tree/master/demo-isubd-terrain#implicit-subdivision-on-the-gpu
  */

#include <bx/allocator.h>
#include <bx/debug.h>
#include <bx/file.h>
#include <bx/math.h>

#include "bgfx_utils.h"
#include "bounds.h"
#include "camera.h"
#include "common.h"
#include "imgui/imgui.h"
#include <bx\rng.h>

namespace
{
	class ExamplePBR_IBL : public entry::AppI
	{
	public:
		ExamplePBR_IBL(const char* _name, const char* _description, const char* _url)
			: entry::AppI(_name, _description, _url)
		{
		}

		bgfx::ProgramHandle compileShader(const char* vsCode, const char* fsCode, const char* defCode)
		{
			if (vsCode == nullptr || fsCode == nullptr || defCode == nullptr)
				return BGFX_INVALID_HANDLE;

			const bgfx::Memory* memVsh = shaderc::compileShader(shaderc::ST_VERTEX, vsCode, "", defCode);
			bgfx::ShaderHandle vsh = bgfx::createShader(memVsh);

			const bgfx::Memory* memFsh = shaderc::compileShader(shaderc::ST_FRAGMENT, fsCode, "", defCode);
			bgfx::ShaderHandle fsh = bgfx::createShader(memFsh);

			return bgfx::createProgram(vsh, fsh, true);
		}

		bgfx::ProgramHandle compileComputeShader(const char* csCode)
		{
			if (csCode == nullptr)
				return BGFX_INVALID_HANDLE;

			const bgfx::Memory* memCs = shaderc::compileShader(shaderc::ST_COMPUTE, csCode);
			if (memCs != nullptr)
			{
				bgfx::ShaderHandle cSh = bgfx::createShader(memCs);
				return bgfx::createProgram(cSh, true);
			}
			else
				return BGFX_INVALID_HANDLE;
		}

		void compileShaders()
		{
		}

		void init(int32_t _argc, const char* const* _argv, uint32_t _width, uint32_t _height) override
		{
			Args args(_argc, _argv);

			m_width = _width;
			m_height = _height;
			m_debug = BGFX_DEBUG_NONE;
			m_reset = BGFX_RESET_VSYNC;

			bgfx::Init init;
			init.type = args.m_type;
			init.vendorId = args.m_pciId;
			init.resolution.width = m_width;
			init.resolution.height = m_height;
			init.resolution.reset = m_reset;
			bgfx::init(init);

			// Enable m_debug text.
			bgfx::setDebug(m_debug);
		}

		virtual int shutdown() override
		{
			return 0;
		}

		bool update() override
		{
			return false;
		}

	public :
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_debug;
		uint32_t m_reset;
	};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	ExamplePBR_IBL
	, "43-PBR_IBL"
	, "PBR_IBL."
	, "https://bkaradzic.github.io/bgfx/examples.html#tess"
	);
