#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/ext.hpp>

#include "forward_shading_common.h"
#include "demos.h"


enum
{
	eModelCount = 1,
	eMaxLightCount = 2,
};

static Dolphin::Model s_model[eModelCount];
static Dolphin::Model s_lights[eMaxLightCount];
static LightData s_lightData[eMaxLightCount];

namespace Dolphin
{
	static PosNormalTexcoordVertex s_vplaneVertices[] =
	{
		{ -1.0f,  1.0f, 0.0f, packF4u(0.0f, 0.0f, -1.0f), 0.0f, 0.0f },
		{  1.0f,  1.0f, 0.0f, packF4u(0.0f, 0.0f, -1.0f), 1.0f, 0.0f },
		{ -1.0f, -1.0f, 0.0f, packF4u(0.0f, 0.0f, -1.0f), 0.0f, 1.0f },
		{  1.0f, -1.0f, 0.0f, packF4u(0.0f, 0.0f, -1.0f), 1.0f, 1.0f },
	};

	static const uint16_t s_planeIndices[] =
	{
		0, 1, 2,
		1, 3, 2,
	};

	static void setDefaultLightState()
	{
		s_lightData[0].rotation = glm::vec3(0.0f, 90.0f, 0.0f);
		s_lightData[0].scale = glm::vec2(29.0f);
		s_lightData[0].intensity = 15.0f;
		s_lightData[0].position = glm::vec3(-104.0f, 16.0f, -3.5f);
		s_lightData[0].twoSided = false;

		s_lightData[1].rotation.y = -90.0f;
		s_lightData[1].scale = glm::vec2(29.0f);
		s_lightData[1].intensity = 10.0f;
		s_lightData[1].position = glm::vec3(90.0f, 16.0f, -3.5f);
		s_lightData[1].textureIdx = 1;
	}

	namespace SponzaDemo
	{
		void init()
		{
			bgfx::VertexLayout PosNormalTexcoordLayout;
			PosNormalTexcoordLayout.begin()
				.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Uint8, true, true)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
				.end();

			s_model[0].loadModel("meshes/morgan-sponza.bin");
			for (int i = 0; i < eMaxLightCount; ++i)
			{
				s_lights[i].loadModel(s_vplaneVertices, BX_COUNTOF(s_vplaneVertices), PosNormalTexcoordLayout, s_planeIndices, BX_COUNTOF(s_planeIndices));
			}

			setDefaultLightState();
		}

		RenderList renderListScene()
		{
			RenderList rlist;
			rlist.models = &s_model[0];
			rlist.count = eModelCount;

			return rlist;
		}

		RenderList renderListLights()
		{
			RenderList rlist;
			rlist.models = &s_lights[0];
			rlist.count = 2;

			return rlist;
		}

		LightData* lightSettings()
		{
			return &s_lightData[0];
		}

		void shutdown()
		{
			for (int i = 0; i < eModelCount; ++i)
			{
				s_model[i].unload();
			}
		}
	}
}
