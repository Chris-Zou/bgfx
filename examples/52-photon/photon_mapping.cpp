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

#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <random>

namespace PhotonMapping
{
	class ExamplePhotonMapping : public entry::AppI
	{
	public:
		ExamplePhotonMapping(const char* _name, const char* _description, const char* _url)
			: entry::AppI(_name, _description, _url)
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
	};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	PhotonMapping::ExamplePhotonMapping
	, "52-Photon Mapping"
	, "Photon Mapping."
	, "https://bkaradzic.github.io/bgfx/examples.html#tess"
);
