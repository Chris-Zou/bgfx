/*
 * Copyright 2019 Daniel Gavin. All rights reserved.
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

#include <debugdraw/debugdraw.h>

#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Renderer/toneMappingRender.h"

namespace Atmosphere
{
	struct ScreenSpaceQuadVertex
	{
		float m_x;
		float m_y;
		float m_z;
		uint32_t m_rgba;
		float m_u;
		float m_v;

		static void init()
		{
			if (isInitialized) {
				return;
			}

			ms_layout
				.begin()
				.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
				.end();
			isInitialized = true;
		}

		void setPosition(float* pos)
		{
			m_x = pos[0];
			m_y = pos[1];
			m_z = pos[2];
		}

		static bool isInitialized;
		static bgfx::VertexLayout ms_layout;
	};

	bool ScreenSpaceQuadVertex::isInitialized = false;
	bgfx::VertexLayout ScreenSpaceQuadVertex::ms_layout;

	static void setFarPlaneScreenSpace()
	{
		float TL[4] = {-1.0f, 1.0f, 1.0f, 1.0f};
		float TR[4] = {1.0f, 1.0f, 1.0f, 1.0f};
		float BL[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
		float BR[4] = {1.0f, -1.0f, 1.0f, 1.0f};

		if (3 == bgfx::getAvailTransientVertexBuffer(3, ScreenSpaceQuadVertex::ms_layout))
		{
			bgfx::TransientVertexBuffer vb;
			bgfx::allocTransientVertexBuffer(&vb, 6, ScreenSpaceQuadVertex::ms_layout);
			ScreenSpaceQuadVertex* vertex = (ScreenSpaceQuadVertex*)vb.data;
			vertex[0].setPosition(TL);
			vertex[0].m_u = 0.0f;
			vertex[0].m_v = 0.0f;
			vertex[0].m_rgba = 0xffffffff;

			vertex[1].setPosition(TR);
			vertex[1].m_u = 1.0f;
			vertex[1].m_v = 0.0f;
			vertex[1].m_rgba = 0xffffffff;

			vertex[2].setPosition(BR);
			vertex[2].m_u = 1.0f;
			vertex[2].m_v = 1.0f;
			vertex[2].m_rgba = 0xffffffff;

			vertex[3].setPosition(BR);
			vertex[3].m_u = 1.0f;
			vertex[3].m_v = 1.0f;
			vertex[3].m_rgba = 0xffffffff;

			vertex[4].setPosition(BL);
			vertex[4].m_u = 0.0f;
			vertex[4].m_v = 1.0f;
			vertex[4].m_rgba = 0xffffffff;

			vertex[5].setPosition(TL);
			vertex[5].m_u = 0.0f;
			vertex[5].m_v = 0.0f;
			vertex[5].m_rgba = 0xffffffff;

			bgfx::setVertexBuffer(0, &vb);
		}
	}

	class AtmosphereScattering : public entry::AppI
	{
	public:
		AtmosphereScattering(const char* _name, const char* _description, const char* _url)
			: entry::AppI(_name, _description, _url)
		{
			m_fPlanetRadius = 6371000.0f;
			m_fAtmosphereHeight = 80000.0f;
			m_fSunIntensity = 1.0f;
			m_fDistanceScale = 1.0f;
			m_fDensityHeight[0] = 7994.0f;
			m_fDensityHeight[1] = 1200.0f;
			m_fDensityHeight[2] = 0.0f;
			m_fDensityHeight[3] = 0.0f;

			m_vDensityScale = bx::Vec3(7994.0f, 1200.0f, 0);
			m_vRayleighSct = bx::Vec3(5.8f * 0.000001f, 13.5f * 0.000001f, 33.1f * 0.000001f);
			m_vMieSct = bx::Vec3(2.0f * 0.000001f, 2.0f * 0.000001f, 2.0f * 0.000001f);

			m_fRayleighScatterCoef = 1.0f;
			m_fRayleighExtinctionCoef = 1.0f;
			m_fMieScatterCoef = 1.0f;
			m_fMieExtinctionCoef = 1.0f;

			m_vScatteringR = m_vRayleighSct;
			m_vExtinctionR = m_vRayleighSct;

			m_vScatteringM = m_vMieSct;
			m_vExtinctionM = m_vMieSct;

			m_fMieG = 0.76f;

			m_lightScale = 1.0f;
		}

		void updateRayleighAndMieCoef()
		{
			m_vScatteringR.x = m_vRayleighSct.x * m_fRayleighScatterCoef;
			m_vScatteringR.y = m_vRayleighSct.y * m_fRayleighScatterCoef;
			m_vScatteringR.z = m_vRayleighSct.z * m_fRayleighScatterCoef;

			m_vExtinctionR.x = m_vRayleighSct.x * m_fRayleighExtinctionCoef;
			m_vExtinctionR.y = m_vRayleighSct.y * m_fRayleighExtinctionCoef;
			m_vExtinctionR.z = m_vRayleighSct.z * m_fRayleighExtinctionCoef;

			m_vScatteringM.x = m_vMieSct.x * m_fMieScatterCoef;
			m_vScatteringM.y = m_vMieSct.y * m_fMieScatterCoef;
			m_vScatteringM.z = m_vMieSct.z * m_fMieScatterCoef;

			m_vExtinctionM.x = m_vMieSct.x * m_fMieExtinctionCoef;
			m_vExtinctionM.y = m_vMieSct.y * m_fMieExtinctionCoef;
			m_vExtinctionM.z = m_vMieSct.z * m_fMieExtinctionCoef;
		}
#pragma region ShaderCompiler
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
			m_atmophereScattering = compileShader("../47-AtmosphereScattering/vs_atmosphere.sc", "../47-AtmosphereScattering/fs_atmosphere.sc", "../47-AtmosphereScattering/varying.def.sc");
		}
#pragma endregion

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

			m_planetRadius = bgfx::createUniform("PlanetRadius", bgfx::UniformType::Vec4);
			m_atmosphereHeight = bgfx::createUniform("AtmosphereHeight", bgfx::UniformType::Vec4);
			m_sunIntensity = bgfx::createUniform("SunIntensity", bgfx::UniformType::Vec4);
			m_distanceScale = bgfx::createUniform("DistanceScale", bgfx::UniformType::Vec4);
			m_densityScaleHeight = bgfx::createUniform("DensityScaleHeight", bgfx::UniformType::Vec4);

			m_scatteringR = bgfx::createUniform("ScatteringR", bgfx::UniformType::Vec4);
			m_scatteringM = bgfx::createUniform("ScatteringM", bgfx::UniformType::Vec4);
			m_extinctionR = bgfx::createUniform("ExtinctionR", bgfx::UniformType::Vec4);
			m_extinctionM = bgfx::createUniform("ExtinctionM", bgfx::UniformType::Vec4);

			m_mieG = bgfx::createUniform("MieG", bgfx::UniformType::Vec4);
			m_incomingLight = bgfx::createUniform("IncomingLight", bgfx::UniformType::Vec4);

			m_cameraPos = bgfx::createUniform("CameraPos", bgfx::UniformType::Vec4);
			m_lightDir = bgfx::createUniform("LightDir", bgfx::UniformType::Vec4);

			m_params = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);

			if (m_bIsFirstFrame)
			{
				compileShaders();
			}

			m_caps = bgfx::getCaps();

			imguiCreate();

			m_aCameraPos[0] = 0.0f;
			m_aCameraPos[1] = 10.0f;
			m_aCameraPos[2] = 0.0f;

			cameraCreate();
			cameraSetPosition({ m_aCameraPos[0], m_aCameraPos[1], m_aCameraPos[2] });
			cameraSetHorizontalAngle(bx::kPi / 2.0);

			ScreenSpaceQuadVertex::init();

			if (bgfx::isValid(m_frameBuffer))
				bgfx::destroy(m_frameBuffer);

			uint32_t msaa = (m_reset & BGFX_RESET_MSAA_MASK) >> BGFX_RESET_MSAA_SHIFT;

			m_fbTexture[0] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::RGBA16F, (uint64_t(msaa + 1) << BGFX_TEXTURE_RT_MSAA_SHIFT) | BGFX_SAMPLER_UVW_CLAMP | BGFX_SAMPLER_POINT);

			const uint64_t textureFlags = BGFX_TEXTURE_RT_WRITE_ONLY | (uint64_t(msaa + 1) << BGFX_TEXTURE_RT_MSAA_SHIFT);

			bgfx::TextureFormat::Enum depthFormat;
			if (bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D24S8, textureFlags))
			{
				depthFormat = bgfx::TextureFormat::D24S8;
			}
			else
			{
				depthFormat = bgfx::TextureFormat::D32;
			}

			m_fbTexture[1] = bgfx::createTexture2D(
				uint16_t(m_width), uint16_t(m_height), false, 1, depthFormat, textureFlags);

			bgfx::setName(m_fbTexture[0], "BaseColor");

			m_frameBuffer = bgfx::createFrameBuffer(BX_COUNTOF(m_fbTexture), m_fbTexture, true);

			m_bIsFirstFrame = false;

			m_vIncomingLight[0] = 2.0f;
			m_vIncomingLight[1] = 2.0f;
			m_vIncomingLight[2] = 2.0f;

			m_vLightDir[0] = 0.0f;
			m_vLightDir[1] = 0.0f;
			m_vLightDir[2] = 1.0f;

			m_vLightDir[3] = 0.76f;

			m_toneMapParams.m_width = m_width;
			m_toneMapParams.m_height = m_height;
			m_toneMapParams.m_originBottomLeft = m_caps->originBottomLeft;
			m_toneMapPass.init(m_caps);
		}

		void setConstantUniforms()
		{
			bgfx::setUniform(m_densityScaleHeight, &m_fDensityHeight);

			bgfx::setUniform(m_scatteringR, &m_vScatteringR);
			bgfx::setUniform(m_scatteringM, &m_vScatteringM);
			bgfx::setUniform(m_extinctionR, &m_vExtinctionR);
			bgfx::setUniform(m_extinctionM, &m_vExtinctionM);

			bgfx::setUniform(m_mieG, &m_fMieG);

			m_aParams[0] = m_fPlanetRadius;
			m_aParams[1] = m_fAtmosphereHeight;
			m_aParams[2] = m_fSunIntensity;
			m_aParams[3] = m_fDistanceScale;

			bgfx::setUniform(m_params, m_aParams);
		}

		void setUniforms()
		{
			bgfx::setUniform(m_incomingLight, &m_vIncomingLight);
			
			bgfx::setUniform(m_lightDir, &m_vLightDir);
		}
		
		virtual int shutdown() override
		{
			bgfx::destroy(m_planetRadius);
			bgfx::destroy(m_atmosphereHeight);
			bgfx::destroy(m_sunIntensity);
			bgfx::destroy(m_distanceScale);
			bgfx::destroy(m_densityScaleHeight);

			bgfx::destroy(m_scatteringR);
			bgfx::destroy(m_scatteringM);
			bgfx::destroy(m_extinctionR);
			bgfx::destroy(m_extinctionM);

			bgfx::destroy(m_mieG);
			bgfx::destroy(m_incomingLight);

			if (bgfx::isValid(m_frameBuffer))
				bgfx::destroy(m_frameBuffer);

			imguiDestroy();

			m_toneMapPass.destroy();

			bgfx::shutdown();

			return 0;
		}

		bool update() override
		{
			if (entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState)) {
				return false;
			}

			if (!bgfx::isValid(m_pbrFrameBuffer) || m_oldWidth != m_width || m_oldHeight != m_height || m_oldReset != m_reset)
			{
				m_oldWidth = m_width;
				m_oldHeight = m_height;
				m_oldReset = m_reset;

				uint32_t msaa = (m_reset & BGFX_RESET_MSAA_MASK) >> BGFX_RESET_MSAA_SHIFT;

				if (bgfx::isValid(m_pbrFrameBuffer))
				{
					bgfx::destroy(m_pbrFrameBuffer);
				}

				m_pbrFBTexture[0] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::RGBA16F, (uint64_t(msaa + 1) << BGFX_TEXTURE_RT_MSAA_SHIFT) | BGFX_SAMPLER_UVW_CLAMP | BGFX_SAMPLER_POINT);
				const uint64_t textureFlags = BGFX_TEXTURE_RT_WRITE_ONLY | (uint64_t(msaa + 1) << BGFX_TEXTURE_RT_MSAA_SHIFT);

				bgfx::TextureFormat::Enum depthFormat;
				if (bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D24S8, textureFlags))
				{
					depthFormat = bgfx::TextureFormat::D24S8;
				}
				else
				{
					depthFormat = bgfx::TextureFormat::D32;
				}

				m_pbrFBTexture[1] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, depthFormat, textureFlags);

				bgfx::setName(m_pbrFBTexture[0], "HDR Color Buffer");
				bgfx::setName(m_pbrFBTexture[1], "Depth Buffer");

				m_pbrFrameBuffer = bgfx::createFrameBuffer(BX_COUNTOF(m_pbrFBTexture), m_pbrFBTexture, true);
			}

			imguiBeginFrame(m_mouseState.m_mx
				, m_mouseState.m_my
				, (m_mouseState.m_buttons[entry::MouseButton::Left] ? IMGUI_MBUT_LEFT : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Right] ? IMGUI_MBUT_RIGHT : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0)
				, m_mouseState.m_mz
				, uint16_t(m_width)
				, uint16_t(m_height)
			);

			ImGui::SetNextWindowPos(
				ImVec2(m_width - m_width / 5.0f - 10.0f, 10.0f)
				, ImGuiCond_FirstUseEver
			);
			ImGui::SetNextWindowSize(
				ImVec2(m_width / 5.0f, m_height / 3.0f)
				, ImGuiCond_FirstUseEver
			);
			ImGui::Begin("Settings"
				, NULL
				, 0
			);

			ImGui::SliderFloat3("CamHeight", m_aCameraPos, 1.0f, 100000.0f);
			ImGui::SliderFloat3("LightDir", m_vLightDir, -1.0f, 1.0f);
			ImGui::SliderFloat("LightIntensity", &m_lightScale, 0.1f, 10.0f);
			m_vIncomingLight[0] = m_lightScale;
			m_vIncomingLight[1] = m_lightScale;
			m_vIncomingLight[2] = m_lightScale;
			ImGui::SliderFloat("MieG", &m_fMieG, 0.01f, 2.0f);
			m_vLightDir[3] = m_fMieG;
			ImGui::SliderFloat("RayScatter", &m_fRayleighScatterCoef, 0.1f, 5.0f);
			ImGui::SliderFloat("MieScatter", &m_fMieScatterCoef, 0.1f, 5.0f);
			ImGui::SliderFloat("RayExtinction", &m_fRayleighExtinctionCoef, 0.1f, 5.0f);
			ImGui::SliderFloat("MieExtinction", &m_fMieExtinctionCoef, 0.1f, 5.0f);
			ImGui::SliderFloat("SunIntensity", &m_fSunIntensity, 0.1f, 10.0f);

			updateRayleighAndMieCoef();

			ImGui::End();

			imguiEndFrame();

			bgfx::setViewFrameBuffer(0, m_pbrFrameBuffer);

			bgfx::touch(0);

			DebugDrawEncoder dde;

			dde.begin(0);
			dde.drawAxis(0.0f, 0.0f, 0.0f);

			int64_t now = bx::getHPCounter();
			static int64_t last = now;
			const int64_t frameTime = now - last;
			last = now;
			const double freq = double(bx::getHPFrequency());
			const float deltaTime = (float)(frameTime / freq);

			//if (m_vLightDir[1] >= -1.0f)
			//{
			//	m_vLightDir[1] -= deltaTime * 0.02f;
			//}

			if (m_aCameraPos[1] < 10000.0f)
			{
				m_aCameraPos[1] += deltaTime * 100.0f;
			}

			float proj[16];
			bx::mtxProj(proj, 60.0f, float(m_width) / float(m_height), 0.1f, 1000.0f, m_caps->homogeneousDepth);

			cameraUpdate(0.1f * deltaTime, m_mouseState);

			float view[16];
			cameraGetViewMtx(view);

			bgfx::setViewRect(0, 0, 0, uint16_t(m_width), uint16_t(m_height));

			bgfx::setViewTransform(0, view, proj);

			cameraSetPosition({ m_aCameraPos[0], m_aCameraPos[1], m_aCameraPos[2] });

			bx::Vec3 cameraPos = cameraGetPosition();
			bgfx::setUniform(m_cameraPos, &cameraPos);

			bgfx::setViewName(0, "AtmosphereScattering");

			uint64_t stateOpaque = 0
				| BGFX_STATE_WRITE_RGB
				| BGFX_STATE_WRITE_A
				| BGFX_STATE_WRITE_Z
				| BGFX_STATE_DEPTH_TEST_ALWAYS
				| BGFX_STATE_CULL_CCW;
			bgfx::setState(stateOpaque);

			setFarPlaneScreenSpace();

			setConstantUniforms();
			setUniforms();

			bgfx::submit(0, m_atmophereScattering);

			bgfx::ViewId toneMappingPass = 1;
			bgfx::setViewName(toneMappingPass, "Tone Mapping");
			bgfx::setViewRect(toneMappingPass, 0, 0, bgfx::BackbufferRatio::Equal);

			m_toneMapPass.render(m_pbrFBTexture[0], m_toneMapParams, deltaTime, toneMappingPass);

			bgfx::frame();

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

		const bgfx::Caps *m_caps;

		bool m_bIsFirstFrame = true;

		bgfx::ProgramHandle m_atmophereScattering;

		bgfx::FrameBufferHandle m_frameBuffer = BGFX_INVALID_HANDLE;

		bgfx::TextureHandle m_fbTexture[2];

#pragma region uniforms
		bgfx::UniformHandle m_planetRadius;
		bgfx::UniformHandle m_atmosphereHeight;
		bgfx::UniformHandle m_sunIntensity;
		bgfx::UniformHandle m_distanceScale;
		bgfx::UniformHandle m_densityScaleHeight;

		bgfx::UniformHandle m_scatteringR;
		bgfx::UniformHandle m_scatteringM;
		bgfx::UniformHandle m_extinctionR;
		bgfx::UniformHandle m_extinctionM;

		bgfx::UniformHandle m_mieG;
		bgfx::UniformHandle m_incomingLight;
		bgfx::UniformHandle m_lightDir;
		bgfx::UniformHandle m_cameraPos;
		bgfx::UniformHandle m_params;
#pragma endregion

#pragma region datas
		float m_fPlanetRadius;
		float m_fAtmosphereHeight;
		float m_fSunIntensity;
		float m_fDistanceScale;
		float m_fDensityHeight[4];

		float m_fRayleighScatterCoef;
		float m_fRayleighExtinctionCoef;
		float m_fMieScatterCoef;
		float m_fMieExtinctionCoef;
		float m_fMieG;

		bx::Vec3 m_vDensityScale;
		bx::Vec3 m_vRayleighSct;
		bx::Vec3 m_vMieSct;
		bx::Vec3 m_vScatteringR;
		bx::Vec3 m_vScatteringM;
		bx::Vec3 m_vExtinctionR;
		bx::Vec3 m_vExtinctionM;

		float m_vIncomingLight[3];
		float m_lightScale;
		float m_vLightDir[4]; // m_vLightDir[3] is MieG

		float m_aParams[4];

		float m_aCameraPos[3];

		bgfx::TextureHandle m_pbrFBTexture[2];
		bgfx::FrameBufferHandle m_pbrFrameBuffer = BGFX_INVALID_HANDLE;
#pragma endregion

		Dolphin::ToneMapParams m_toneMapParams;
		Dolphin::ToneMapping m_toneMapPass;
	};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	Atmosphere::AtmosphereScattering
	, "47-AtmosphereScattering"
	, "AtmosphereScattering."
	, "https://bkaradzic.github.io/bgfx/examples.html#tess"
);
