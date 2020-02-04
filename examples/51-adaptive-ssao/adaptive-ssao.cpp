/*
 * Copyright 2019 Zou Pan Pan. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
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
#include <bx/rng.h>
#include <bx/os.h>

#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <random>

#include "SceneManager/sceneMangement.h"
#include "SceneManager/lightVolumeShape.h"
#include "Renderer/toneMappingRender.h"
#include "ShaderCompiler/ShaderCompiler.h"
#include "SceneManager/gltf_model_loading.h"

namespace ASSAO
{

#define RENDER_PASS_GBUGGER 0
#define RENDER_PASS_COMBINE 1

#define GBUFFER_RT_NORMAL 0
#define GBUFFER_RT_COLOR  1
#define GBUFFER_RT_DEPTH  2

	bgfx::ProgramHandle compileSingleGraphicsProgram(const std::string& prefix, const std::string& vsName, const std::string& fsName)
	{
		std::string vsPath = prefix + vsName + ".sc";
		std::string fsPath = prefix + fsName + ".sc";
		std::string defPath = prefix + "varying.def.sc";

		return Dolphin::compileGraphicsShader(vsPath.c_str(), fsPath.c_str(), defPath.c_str());
	}

	bgfx::ProgramHandle compileSigleComputeProgram(const std::string& prefix, const std::string& csName)
	{
		std::string csPath = prefix + csName + ".sc";
		return Dolphin::compileComputeShader(csPath.c_str());
	}

	class ExampleAdaptiveSSAO : public entry::AppI
	{
	public:
		ExampleAdaptiveSSAO(const char* _name, const char* _description, const char* _url)
			: entry::AppI(_name, _description, _url)
		{
		}

		void compileNeededShaders()
		{
			std::string prefix("../51-adaptive-ssao/");
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

			bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL, 0x000000ff, 1.0f, 0);
		}

		virtual int shutdown() override
		{
			bgfx::shutdown();

			return 0;
		}

		bool update() override
		{
			return true;
		}

	public:
		entry::MouseState m_mouseState;

		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_debug;
		uint32_t m_reset;

		uint32_t m_oldWidth;
		uint32_t m_oldHeight;
		uint32_t m_oldReset;
	};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	ASSAO::ExampleAdaptiveSSAO
	, "51-assao"
	, "Adaptive Screen Space Ambient Occlusion."
	, "https://bkaradzic.github.io/bgfx/examples.html#tess"
);
