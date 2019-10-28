#include <string>
#include <vector>
#include <algorithm>

#include "common.h"

#include <imgui/imgui.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

using namespace glm;

#include <bgfx/bgfx.h>
#include <bx/timer.h>
#include <bx/math.h>


#include "forward_shading_common.h"
#include "demos.h"

#include "ShaderCompiler/ShaderCompiler.h"

#define RENDERVIEW_DRAWSCENE_0_ID 1

static bool s_flipV = false;

static bgfx::FrameBufferHandle s_rtColorBuffer;
static bgfx::TextureHandle s_rtColorTexture;

static GlobalRenderingData s_gData;

std::map<std::string, bgfx::TextureHandle> Dolphin::Mesh::m_textureCache;

static RenderState s_renderStates[RenderState::Count] =
{
	{ // Default
		0
		| BGFX_STATE_WRITE_RGB
		| BGFX_STATE_WRITE_A
		| BGFX_STATE_DEPTH_TEST_LEQUAL
		| BGFX_STATE_WRITE_Z
		| BGFX_STATE_CULL_CCW
		| BGFX_STATE_MSAA
		, UINT32_MAX
		, BGFX_STENCIL_NONE
		, BGFX_STENCIL_NONE
	},
	{ // ZPass
		0
		| BGFX_STATE_WRITE_Z
		| BGFX_STATE_DEPTH_TEST_LEQUAL
		| BGFX_STATE_CULL_CCW
		| BGFX_STATE_MSAA
		, UINT32_MAX
		, BGFX_STENCIL_NONE
		, BGFX_STENCIL_NONE
	},
	{ // ZTwoSidedPass
		0
		| BGFX_STATE_WRITE_Z
		| BGFX_STATE_DEPTH_TEST_LEQUAL
		| BGFX_STATE_MSAA
		, UINT32_MAX
		, BGFX_STENCIL_NONE
		, BGFX_STENCIL_NONE
	},
	{ // ColorPass
		0
		| BGFX_STATE_WRITE_RGB
		| BGFX_STATE_WRITE_A
		| BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE)
		| BGFX_STATE_DEPTH_TEST_EQUAL
		| BGFX_STATE_CULL_CCW
		| BGFX_STATE_MSAA
		, UINT32_MAX
		, BGFX_STENCIL_NONE
		, BGFX_STENCIL_NONE
	},
	{ // ColorAlphaPass
		0
		| BGFX_STATE_WRITE_RGB
		| BGFX_STATE_WRITE_A
		| BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE)
		| BGFX_STATE_DEPTH_TEST_EQUAL
		| BGFX_STATE_MSAA
		, UINT32_MAX
		, BGFX_STENCIL_NONE
		, BGFX_STENCIL_NONE
	}
};

bgfx::VertexLayout PosColorTexCoord0Vertex::ms_layout;

struct Programs
{
	void init()
	{
		m_pbrShader = Dolphin::compileGraphicsShader("../48-DolphinEngine/vs_pbr.sc", "../48-DolphinEngine/fs_pbr.sc", "../48-DolphinEngine/varying.def.sc");
		m_pbrShaderWithMask = Dolphin::compileGraphicsShader("../48-DolphinEngine/vs_pbr.sc", "../48-DolphinEngine/fs_pbr_masked.sc", "../48-DolphinEngine/varying.def.sc");
		m_blit = Dolphin::compileGraphicsShader("../48-DolphinEngine/vs_blit.sc", "../48-DolphinEngine/fs_blit.sc", "../48-DolphinEngine/varying.def.sc");
	}

	void destroy()
	{
		bgfx::destroy(m_pbrShader);
		bgfx::destroy(m_pbrShaderWithMask);
	}

	bgfx::ProgramHandle m_blit;
	bgfx::ProgramHandle m_pbrShader;
	bgfx::ProgramHandle m_pbrShaderWithMask;
};
static Programs s_programs;

void screenSpaceQuad(bool _originBottomLeft = true, float zz = 0.0f, float _width = 1.0f, float _height = 1.0f)
{
	if (bgfx::getAvailTransientVertexBuffer(3, PosColorTexCoord0Vertex::ms_layout))
	{
		bgfx::TransientVertexBuffer vb;
		bgfx::allocTransientVertexBuffer(&vb, 3, PosColorTexCoord0Vertex::ms_layout);
		PosColorTexCoord0Vertex* vertex = (PosColorTexCoord0Vertex*)vb.data;

		const float minx = -_width;
		const float maxx = _width;
		const float miny = 0.0f;
		const float maxy = _height * 2.0f;

		const float minu = -1.0f;
		const float maxu = 1.0f;

		float minv = 0.0f;
		float maxv = 2.0f;

		if (_originBottomLeft)
		{
			std::swap(minv, maxv);
			minv -= 1.0f;
			maxv -= 1.0f;
		}

		vertex[0].m_x = minx;
		vertex[0].m_y = miny;
		vertex[0].m_z = zz;
		vertex[0].m_rgba = 0xffffffff;
		vertex[0].m_u = minu;
		vertex[0].m_v = minv;

		vertex[1].m_x = maxx;
		vertex[1].m_y = miny;
		vertex[1].m_z = zz;
		vertex[1].m_rgba = 0xffffffff;
		vertex[1].m_u = maxu;
		vertex[1].m_v = minv;

		vertex[2].m_x = maxx;
		vertex[2].m_y = maxy;
		vertex[2].m_z = zz;
		vertex[2].m_rgba = 0xffffffff;
		vertex[2].m_u = maxu;
		vertex[2].m_v = maxv;

		bgfx::setVertexBuffer(0, &vb);
	}
}

glm::mat4 SetLightUniforms(const LightData& l)
{
	// setup light transform
	glm::mat4 identity(1.0f);

	glm::mat4 scale = glm::scale(identity, glm::vec3(0.5f*l.scale, 1.0f));
	glm::mat4 translate = glm::translate(identity, l.position);
	glm::mat4 rotateZ = glm::rotate(identity, glm::radians(l.rotation.z), glm::vec3(0, 0, -1));
	glm::mat4 rotateY = glm::rotate(identity, glm::radians(l.rotation.y), glm::vec3(0, -1, 0));
	glm::mat4 rotateX = glm::rotate(identity, glm::radians(l.rotation.x), glm::vec3(-1, 0, 0));

	glm::mat4 lightTransform = translate * rotateX*rotateY*rotateZ*scale;

	s_gData.m_uniforms.m_quadPoints[0] = lightTransform * glm::vec4(-1, 1, 0, 1);
	s_gData.m_uniforms.m_quadPoints[1] = lightTransform * glm::vec4(1, 1, 0, 1);
	s_gData.m_uniforms.m_quadPoints[2] = lightTransform * glm::vec4(1, -1, 0, 1);
	s_gData.m_uniforms.m_quadPoints[3] = lightTransform * glm::vec4(-1, -1, 0, 1);

	// set the light state
	s_gData.m_uniforms.m_lightIntensity = l.intensity;
	s_gData.m_uniforms.m_twoSided = l.twoSided;

	s_gData.m_uniforms.submitPerLightUniforms();

	return lightTransform;
}

int _main_(int _argc, char** _argv)
{
	Args args(_argc, _argv);

	uint32_t debug = BGFX_DEBUG_TEXT;
	uint32_t reset = BGFX_RESET_MAXANISOTROPY | BGFX_RESET_VSYNC;

	ViewState viewState(1280, 720);
	ClearValues clearValues(0x00000000, 1.0f, 0);

	bgfx::Init init;
	init.type = args.m_type;
	init.vendorId = args.m_pciId;
	init.resolution.width = viewState.m_width;
	init.resolution.height = viewState.m_height;
	init.resolution.reset = reset;
	bgfx::init(init);

	bgfx::setDebug(debug);

	bgfx::RendererType::Enum type = bgfx::getRendererType();

	switch (type)
	{
	case bgfx::RendererType::OpenGLES:
	case bgfx::RendererType::OpenGL:
		s_flipV = true;
		break;

	default:
		break;
	}

	imguiCreate();

	s_gData.m_uniforms.init();

	Dolphin::SponzaDemo::init();

	s_programs.init();

	bgfx::VertexLayout posLayout;
	posLayout.begin();
	posLayout.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);
	posLayout.end();

	PosColorTexCoord0Vertex::init();

	const uint32_t samplerFlags = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;

	struct SceneSettings
	{
		float    m_diffColor[3];
		float    m_roughness;
		float    m_reflectance;
		float    m_jitterAASigma;
		uint32_t m_sampleCount;
		uint32_t m_demoIdx;
		uint32_t m_currLightIdx;
		bool     m_useGT;
		bool     m_showDiffColor;
	};

	SceneSettings settings;
	settings.m_diffColor[0] = 1.0f;
	settings.m_diffColor[1] = 1.0f;
	settings.m_diffColor[2] = 1.0f;
	settings.m_roughness = 1.0f;
	settings.m_reflectance = 0.04f;
	settings.m_currLightIdx = 0;
	settings.m_jitterAASigma = 0.3f;
	settings.m_sampleCount = 0;
	settings.m_demoIdx = 1;
	settings.m_useGT = false;
	settings.m_showDiffColor = false;

	float prevDiffColor[3] = { 0 };

	s_rtColorBuffer.idx = bgfx::kInvalidHandle;
	s_rtColorTexture.idx = bgfx::kInvalidHandle;

	float initialPrimPos[3] = { 0.0f, 60.0f, -60.0f };
	bx::Vec3 initialSponzaPos = { 0.0f, 20.0f, 0.0f };
	//float initialPrimVertAngle = -0.45f;
	float initialSponzaVertAngle = 0.0f;
	//float initialPrimHAngle = 0.0f;
	float initialSponzaHAngle = -1.54f;
	cameraCreate();
	cameraSetPosition(initialSponzaPos);
	cameraSetHorizontalAngle(initialSponzaHAngle);
	cameraSetVerticalAngle(initialSponzaVertAngle);

	const float camFovy = 60.0f;
	const float camAspect = float(int32_t(viewState.m_width)) / float(int32_t(viewState.m_height));
	const float camNear = 0.1f;
	const float camFar = 2000.0f;
	bx::mtxProj(viewState.m_proj, camFovy, camAspect, camNear, camFar, bgfx::getCaps()->homogeneousDepth);
	cameraGetViewMtx(viewState.m_view);

	entry::MouseState mouseState;
	while (!entry::processEvents(viewState.m_width, viewState.m_height, debug, reset, &mouseState))
	{
		Dolphin::RenderList rlist;
		//Dolphin::RenderList llist;
		LightData* lsettings;

		rlist = Dolphin::SponzaDemo::renderListScene();
		//llist = Dolphin::SponzaDemo::renderListLights();
		lsettings = Dolphin::SponzaDemo::lightSettings();

		bool rtRecreated = false;

		if (viewState.m_oldWidth != viewState.m_width
			|| viewState.m_oldHeight != viewState.m_height
			|| viewState.m_oldReset != reset)
		{
			viewState.m_oldWidth = viewState.m_width;
			viewState.m_oldHeight = viewState.m_height;
			viewState.m_oldReset = reset;

			if (bgfx::isValid(s_rtColorBuffer))
			{
				bgfx::destroy(s_rtColorBuffer);
			}

			uint32_t w = viewState.m_width;
			uint32_t h = viewState.m_height;

			// Note: bgfx will cap the quality to the maximum supported
			uint64_t rtFlags = BGFX_TEXTURE_RT;

			s_rtColorTexture = bgfx::createTexture2D(uint16_t(w), uint16_t(h), false, 1, bgfx::TextureFormat::RGBA32F, rtFlags);
			bgfx::TextureHandle colorTextures[] =
			{
				s_rtColorTexture,
				bgfx::createTexture2D(uint16_t(w), uint16_t(h), false, 1, bgfx::TextureFormat::D24S8,   rtFlags)
			};
			s_rtColorBuffer = bgfx::createFrameBuffer(BX_COUNTOF(colorTextures), colorTextures, true);
			rtRecreated = true;
		}
		{
			//imguiEndFrame();
		}

		int64_t now = bx::getHPCounter();
		static int64_t last = now;
		const int64_t frameTime = now - last;
		last = now;
		const double freq = double(bx::getHPFrequency());
		const double toMs = 1000.0 / freq;
		const float deltaTime = float(frameTime / freq);

		bgfx::dbgTextClear();
		bgfx::dbgTextPrintf(0, 1, 0x4f, "bgfx/examples/xx-arealights");
		bgfx::dbgTextPrintf(0, 2, 0x6f, "Description: Area lights example.");
		bgfx::dbgTextPrintf(0, 3, 0x0f, "Frame: % 7.3f[ms]", double(frameTime)*toMs);

		cameraUpdate(deltaTime, mouseState);

		cameraGetViewMtx(viewState.m_view);

		if (memcmp(viewState.m_oldView, viewState.m_view, sizeof(viewState.m_view))
			|| memcmp(prevDiffColor, settings.m_diffColor, sizeof(settings.m_diffColor))
			|| rtRecreated)
		{
			memcpy(viewState.m_oldView, viewState.m_view, sizeof(viewState.m_view));
			memcpy(prevDiffColor, settings.m_diffColor, sizeof(settings.m_diffColor));
			settings.m_sampleCount = 0;
		}

		// Grab camera position
		bx::Vec3 viewPos;
		viewPos = cameraGetPosition();

		// Compute transform matrices.
		float screenProj[16];
		float screenView[16];
		bx::mtxIdentity(screenView);
		bx::mtxOrtho(screenProj, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);

		float proj[16];
		memcpy(proj, viewState.m_proj, sizeof(proj));
		bgfx::setViewTransform(0, viewState.m_view, proj);

		// Setup views and render targets.
		bgfx::setViewRect(0, 0, 0, uint16_t(viewState.m_width), uint16_t(viewState.m_height));

		bgfx::setViewClear(0
			, BGFX_CLEAR_COLOR
			| BGFX_CLEAR_DEPTH
			, clearValues.m_clearRgba
			, clearValues.m_clearDepth
			, clearValues.m_clearStencil
		);
		bgfx::touch(0);

		uint8_t passViewID = RENDERVIEW_DRAWSCENE_0_ID;

		//color pass
		{
			bgfx::setViewFrameBuffer(passViewID, s_rtColorBuffer);
			bgfx::setViewName(passViewID, "Color Pass");
			bgfx::setViewRect(passViewID, 0, 0, uint16_t(viewState.m_width), uint16_t(viewState.m_height));

			uint32_t flagsRT = BGFX_CLEAR_DEPTH;
			if (settings.m_sampleCount == 0)
			{
				flagsRT |= BGFX_CLEAR_COLOR;
			}

			bgfx::setViewClear(passViewID
				, uint16_t(flagsRT)
				, clearValues.m_clearRgba
				, clearValues.m_clearDepth
				, clearValues.m_clearStencil
			);
			bgfx::touch(passViewID);

			bgfx::setViewTransform(passViewID, viewState.m_view, proj);
			for (uint64_t renderIdx = 0; renderIdx < rlist.count; ++renderIdx)
			{
				rlist.models[renderIdx].submit(s_gData, passViewID, s_programs.m_pbrShader, s_renderStates[RenderState::ColorPass]);
			}

			settings.m_sampleCount += NUM_SAMPLES;
			++passViewID;
		}

		// Blit pass
		uint8_t blitID = passViewID;
		bgfx::FrameBufferHandle handle = BGFX_INVALID_HANDLE;
		bgfx::setViewFrameBuffer(blitID, handle);
		bgfx::setViewRect(blitID, 0, 0, uint16_t(viewState.m_width), uint16_t(viewState.m_height));
		bgfx::setViewTransform(blitID, screenView, screenProj);
		bgfx::setViewName(blitID, "tone mapping");

		bgfx::setTexture(2, s_gData.m_uColorMap, s_rtColorTexture);
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		screenSpaceQuad(s_flipV);
		bgfx::submit(blitID, s_programs.m_blit);

		bgfx::frame();
	}

	Dolphin::SponzaDemo::shutdown();

	bgfx::destroy(s_rtColorBuffer);

	s_programs.destroy();

	cameraDestroy();
	imguiDestroy();

	bgfx::shutdown();

	return 0;
}
