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

#include "Include/scene.h"
#include "Include/pinhole.h"
#include "Include/plane.h"
#include "Include/sphere.h"
#include "Include/pointLight.h"

namespace PhotonMapping
{
	class ExamplePhotonMapping : public entry::AppI
	{
	public:
		ExamplePhotonMapping(const char* _name, const char* _description, const char* _url)
			: entry::AppI(_name, _description, _url)
		{
			m_image = nullptr;
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

			bgfx::setDebug(m_debug);

			m_cornellBox.SetCamera(new Pinhole(Vector(0, 1, 0), Vector(1, 0, 0), Vector(0, 0, 1), Vector(0, 0.25f, -1.7f), PI / 4, 1.0, _width, _height));

			Plane leftWall(Vector(-1, 0, 0), Vector(1, 0, 0));
			leftWall.SetMaterial(new Material(RED, BLACK, BLACK, BLACK, 0.0f));
			m_cornellBox.AddShape(&leftWall);

			Plane rightWall(Plane(Vector(1, 0, 0), Vector(-1, 0, 0)));
			rightWall.SetMaterial(new Material(GREEN, BLACK, BLACK, BLACK, 0.0f));
			m_cornellBox.AddShape(&rightWall);

			m_cornellBox.AddShape(new Plane(Vector(0, 1, 0), Vector(0, -1, 0)));
			m_cornellBox.AddShape(new Plane(Vector(0, -0.25f, 0), Vector(0, 1, 0)));
			m_cornellBox.AddShape(new Plane(Vector(0, 0, 1), Vector(0, 0, -1)));

			PhotonSphere yellowSphere(Vector(-0.45f, 0.1f, 0.4f), 0.25f);
			yellowSphere.SetMaterial(new Material(YELLOW, GRAY / 4.0f, BLACK, BLACK, 1.5f));
			m_cornellBox.AddShape(&yellowSphere);

			PhotonSphere purpleSphere(Vector(0.45f, 0.1f, 0.4f), 0.25f);
			purpleSphere.SetMaterial(new Material(BLACK, BLACK, PURPLE, BLACK, 0.0f));
			m_cornellBox.AddShape(&purpleSphere);

			m_cornellBox.AddLightSource(new PointLight(Vector(0.0f, 0.6f, -0.1f), 1.6f, WHITE));

			m_cornellBox.SetEmitedPhotons(100000);
			m_cornellBox.SetKNearestNeighbours(300);
			m_cornellBox.SetSpecularSteps(1);

			m_cornellBox.EmitPhotons();

			m_image = m_cornellBox.RenderMultiThread();
			//m_image = m_cornellBox.RenderSceneDepth();
			if(m_image != nullptr)
				m_image->SaveBMP("corrnelBox.bmp");
		}

		virtual int shutdown() override
		{
			bgfx::shutdown();

			return 0;
		}

		bool update() override
		{
			return false;
		}

	public:
		entry::MouseState m_mouseState;

		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_debug;
		uint32_t m_reset;

		Scene m_cornellBox;
		Image* m_image;
	};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	PhotonMapping::ExamplePhotonMapping
	, "52-Photon Mapping"
	, "Photon Mapping."
	, "https://bkaradzic.github.io/bgfx/examples.html#tess"
);
