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
#include <bx/rng.h>

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
			m_fDensityHeight = 1.0f;

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
			m_aCameraPos[1] = -5.0f;
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

			showExampleDialog(this);

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

			ImGui::SliderFloat3("CamHeight", m_aCameraPos, -100.0f, 100.0f);
			ImGui::SliderFloat3("LightDir", m_vLightDir, -1.0f, 1.0f);
			ImGui::SliderFloat("LightIntensity", &m_lightScale, 0.1f, 10.0f);
			m_vIncomingLight[0] = m_lightScale;
			m_vIncomingLight[1] = m_lightScale;
			m_vIncomingLight[2] = m_lightScale;
			ImGui::SliderFloat("MieG", &m_fMieG, 0.01f, 2.0f);

			ImGui::End();

			imguiEndFrame();

			bgfx::setViewFrameBuffer(0, m_pbrFrameBuffer);

			bgfx::touch(0);

			int64_t now = bx::getHPCounter();
			static int64_t last = now;
			const int64_t frameTime = now - last;
			last = now;
			const double freq = double(bx::getHPFrequency());
			const float deltaTime = (float)(frameTime / freq);

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
		float m_fDensityHeight;

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
		float m_vLightDir[3];

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
/*

using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;
using System;
using System.Text;

[RequireComponent(typeof(Camera))]
public class AtmoshpereScattering : MonoBehaviour {
	public enum RenderMode
	{
		Reference,
		Optimized
	}

	public enum LightShaftsQuality
	{
		High,
		Medium
	}

	private Camera _camera;

	[ColorUsageAttribute(false, true)]
	public Color IncomingLight = new Color(4, 4, 4, 4);

	[Range(0, 10.0f)]
	public float RayleighScatterCoef = 1;
	[Range(0, 10.0f)]
	public float RayleighExtinctionCoef = 1;
	[Range(0, 10.0f)]
	public float MieScatterCoef = 1;
	[Range(0, 10.0f)]
	public float MieExtinctionCoef = 1;
	[Range(0.0f, 0.999f)]
	public float MieG = 0.76f;
	public float DistanceScale = 1;

	public float SunIntensity = 1;

	private const float AtmosphereHeight = 80000.0f;    //meters
	private const float PlanetRadius = 6371000.0f;
	private readonly Vector4 DensityScale = new Vector4(7994.0f, 1200.0f, 0, 0);
	private readonly Vector4 RayleighSct = new Vector4(5.8f, 13.5f, 33.1f, 0.0f) * 0.000001f;
	private readonly Vector4 MieSct = new Vector4(2.0f, 2.0f, 2.0f, 0.0f) * 0.00001f;

	private Material _material = null;
	private RenderTexture _lightColorTexture;
	private Texture2D _lightColorTextureTmp;
	private Color[] _directionalLightLUT;
	private Color[] _ambientLightLUT;

	private const int LightLUTSize = 128;

	private Material _lightShaftMaterial = null;
	private CommandBuffer _lightShaftsCommandBuffer;
	private CommandBuffer _cascadeShadowCommandBuffer;

	public Light Sun;

	private Vector4[] _FrustumCorners = new Vector4[4];

	[Range(1, 64)]
	public int SampleCount = 16;

	private void Start()
	{
		_camera = GetComponent<Camera>();
		Shader _shader = Shader.Find("Hidden/AtmosphereScattering");
		if (_shader == null)
			throw new Exception("Critical Error: \"Hidden/AtmosphericScattering\" shader is missing. Make sure it is included in \"Always Included Shaders\" in ProjectSettings/Graphics.");

		_material = new Material(_shader);

		_shader = Shader.Find("Hidden/LightShafts");
		if (_shader == null)
			throw new Exception("Critical Error: \"Hidden/LightShafts\" shader is missing. Make sure it is included in \"Always Included Shaders\" in ProjectSettings/Graphics.");
		_lightShaftMaterial = new Material(_shader);

		//init light shafts
		if(_cascadeShadowCommandBuffer == null)
		{
			_cascadeShadowCommandBuffer = new CommandBuffer();
			_cascadeShadowCommandBuffer.name = "CascadeShadowCommandBuffer";
			_cascadeShadowCommandBuffer.SetGlobalTexture("_CascadeShadowMapTexture", new UnityEngine.Rendering.RenderTargetIdentifier(UnityEngine.Rendering.BuiltinRenderTextureType.CurrentActive));
		}

		if(_lightShaftsCommandBuffer == null)
		{
			_lightShaftsCommandBuffer = new CommandBuffer();
			_lightShaftsCommandBuffer.name = "LightShaftsCommandBuffer";
		}

		int lightShaftsRT1 = Shader.PropertyToID("_LightShaft1");
		Texture nullTexture = null;

		_lightShaftsCommandBuffer.GetTemporaryRT(lightShaftsRT1, _camera.pixelWidth, _camera.pixelHeight, 0, FilterMode.Bilinear, RenderTextureFormat.RHalf);
		_lightShaftsCommandBuffer.Blit(nullTexture, new RenderTargetIdentifier(lightShaftsRT1), _lightShaftMaterial);


		if(_cascadeShadowCommandBuffer != null)
			Sun.AddCommandBuffer(LightEvent.AfterShadowMap, _cascadeShadowCommandBuffer);

		if(_lightShaftsCommandBuffer != null)
			Sun.AddCommandBuffer(LightEvent.BeforeScreenspaceMask, _lightShaftsCommandBuffer);
	}

	private void UpdateMaterialParameters(Material material)
	{
		material.SetFloat("_AtmosphereHeight", AtmosphereHeight);
		material.SetFloat("_PlanetRadius", PlanetRadius);
		material.SetVector("_DensityScaleHeight", DensityScale);

		material.SetVector("_ScatteringR", RayleighSct * RayleighScatterCoef);
		material.SetVector("_ScatteringM", MieSct * MieScatterCoef);
		material.SetVector("_ExtinctionR", RayleighSct * RayleighExtinctionCoef);
		material.SetVector("_ExtinctionM", MieSct * MieExtinctionCoef);

		material.SetColor("_IncomingLight", IncomingLight);
		material.SetFloat("_MieG", MieG);
	}

	private void CalculateLightLUTs()
	{
		if(_lightColorTexture == null)
		{
			_lightColorTexture = new RenderTexture(LightLUTSize, 1, 0, RenderTextureFormat.ARGBHalf, RenderTextureReadWrite.Linear);
			_lightColorTexture.name = "LightColorTexture";
			_lightColorTexture.Create();
		}

		if(_lightColorTextureTmp == null)
		{
			_lightColorTextureTmp = new Texture2D(LightLUTSize, 1, TextureFormat.RGBAHalf, false, true);
			_lightColorTextureTmp.name = "LightColorTextureTmp";
			_lightColorTextureTmp.Apply();
		}
	}

	private void Update()
	{
		_FrustumCorners[0] = _camera.ViewportToWorldPoint(new Vector3(0, 0, _camera.farClipPlane));
		_FrustumCorners[1] = _camera.ViewportToWorldPoint(new Vector3(0, 1, _camera.farClipPlane));
		_FrustumCorners[2] = _camera.ViewportToWorldPoint(new Vector3(1, 1, _camera.farClipPlane));
		_FrustumCorners[3] = _camera.ViewportToWorldPoint(new Vector3(1, 0, _camera.farClipPlane));

		_lightShaftMaterial.SetVectorArray("_FrustumCorners", _FrustumCorners);
		_lightShaftMaterial.SetInt("_SampleCount", SampleCount);

		UpdateMaterialParameters(_material);
		_material.SetVectorArray("_FrustumCorners", _FrustumCorners);
		_material.SetFloat("_SunIntensity", SunIntensity);
		_material.SetColor("_IncomingLight", IncomingLight);
		_material.SetFloat("_MieG", MieG);
		_material.SetFloat("_DistanceScale", DistanceScale);

	}

	private void OnPreRender()
	{
		if (RenderSettings.skybox != null)
		{
			RenderSettings.skybox.SetVector("_CameraPos", _camera.transform.position);
			UpdateMaterialParameters(RenderSettings.skybox);

			RenderSettings.skybox.SetFloat("_SunIntensity", SunIntensity);

		}
	}

	[ImageEffectOpaque]
	public void OnRenderImage(RenderTexture source, RenderTexture destination)
	{
		_material.SetTexture("_Background", source);
		Texture nullTexture = null;
		Graphics.Blit(nullTexture, destination, _material, 0);
		//Graphics.Blit(source, destination);
	}
}

///////////////////Shader//////////////////////
Shader "Hidden/AtmosphereScattering"
{
	Properties
	{
		_MainTex ("Texture", 2D) = "white" {}
		_ZTest("ZTest", Float) = 0
	}
	SubShader
	{
		Tags { "RenderType"="Opaque" }
		LOD 100

		Pass
		{
			CGPROGRAM
			#pragma vertex vert
			#pragma fragment frag


			#include "UnityCG.cginc"
			#include "UnityDeferredLibrary.cginc"

			#define PI 3.14159265359

			float4 _FrustumCorners[4];

			float _PlanetRadius;
			float _AtmosphereHeight;
			float _SunIntensity;
			float _DistanceScale;
			float2 _DensityScaleHeight;

			float3 _ScatteringR;
			float3 _ScatteringM;
			float3 _ExtinctionR;
			float3 _ExtinctionM;

			float _MieG;
			float4 _IncomingLight;

			sampler2D _LightShaft1;
			sampler2D _Background;

			struct VSInput
			{
				float4 vertex : POSITION;
				float2 uv : TEXCOORD0;
				uint vertexId : SV_VertexID;
			};

			struct PSInput
			{
				float4 pos : SV_POSITION;
				float2 uv : TEXCOORD0;
				float3 wpos : TEXCOORD1;
			};

			PSInput vert (VSInput i)
			{
				PSInput o;

				o.pos = UnityObjectToClipPos(i.vertex);
				o.uv = i.uv;
				o.wpos = _FrustumCorners[i.vertexId];

				return o;
			}

			float2 RaySphereIntersection(float3 rayOrigin, float3 rayDir, float3 sphereCenter, float sphereRadius)
			{
				rayOrigin -= sphereCenter;
				float a = dot(rayDir, rayDir);
				float b = 2.0 * dot(rayOrigin, rayDir);
				float c = dot(rayOrigin, rayOrigin) - (sphereRadius * sphereRadius);
				float d = b * b - 4 * a * c;
				if (d < 0)
				{
					return -1;
				}
				else
				{
					d = sqrt(d);
					return float2(-b - d, -b + d) / (2 * a);
				}
			}

			float2 PrecomputeParticleDensity(float3 rayStart, float3 rayDir)
			{
				float3 planetCenter = float3(0, -_PlanetRadius, 0);

				float stepCount = 250;

				float2 intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, _PlanetRadius);
				if (intersection.x > 0)
				{
					// intersection with planet, write high density
					return 1e+20;
				}

				intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, _PlanetRadius + _AtmosphereHeight);
				float3 rayEnd = rayStart + rayDir * intersection.y;

				// compute density along the ray
				float3 step = (rayEnd - rayStart) / stepCount;
				float stepSize = length(step);
				float2 density = 0;

				for (float s = 0.5; s < stepCount; s += 1.0)
				{
					float3 position = rayStart + step * s;
					float height = abs(length(position - planetCenter) - _PlanetRadius);
					float2 localDensity = exp(-(height.xx / _DensityScaleHeight));

					density += localDensity * stepSize;
				}

				return density;
			}

			void GetAtmosphereDensity(float3 position, float3 planetCenter, float3 lightDir, out float2 localDensity, out float2 densityToAtmTop)
			{
				float height = length(position - planetCenter) - _PlanetRadius;
				localDensity = exp(-height.xx / _DensityScaleHeight.xy);

				float cosAngle = dot(normalize(position - planetCenter), -lightDir.xyz);
				float sinAngle = sqrt(saturate(1.0f - cosAngle * cosAngle));
				float3 rayDir = float3(sinAngle, cosAngle, 0);
				float3 rayStart = float3(0, height, 0);

				densityToAtmTop = PrecomputeParticleDensity(rayStart, rayDir);
			}

			void ComputeLocalInscattering(float2 localDensity, float2 densityPA, float2 densityCP, out float3 localInscatterR, out float3 localInscatterM)
			{
				float2 densityCPA = densityCP + densityPA;

				float3 Tr = densityCPA.x * _ExtinctionR;
				float3 Tm = densityCPA.y * _ExtinctionM;

				float3 extinction = exp(-(Tr + Tm));

				localInscatterR = localDensity.x * extinction;
				localInscatterM = localDensity.y * extinction;
			}

			float Sun(float cosAngle)
			{
				float g = 0.98;
				float g2 = g * g;

				float sun = pow(1 - g, 2.0) / (4 * PI * pow(1.0 + g2 - 2.0*g*cosAngle, 1.5));
				return sun * 0.003;// 5;
			}

			float3 RenderSun(in float3 scatterM, float cosAngle)
			{
				return scatterM * Sun(cosAngle);
			}

			void ApplyPhaseFunction(inout float3 scatterR, inout float3 scatterM, float cosAngle)
			{
				// r
				float phase = (3.0 / (16.0 * PI)) * (1 + (cosAngle * cosAngle));
				scatterR *= phase;

				// m
				float g = _MieG;
				float g2 = g * g;
				phase = (1.0 / (4.0 * PI)) * ((3.0 * (1.0 - g2)) / (2.0 * (2.0 + g2))) * ((1 + cosAngle * cosAngle) / (pow((1 + g2 - 2 * g*cosAngle), 3.0 / 2.0)));
				scatterM *= phase;
			}

			float4 IntegrateInscattering(float3 rayStart, float3 rayDir, float rayLength, float3 planetCenter, float distanceScale, float3 lightDir, float sampleCount, out float4 extinction)
			{
				float3 step = rayDir * (rayLength / sampleCount);
				float stepSize = length(step) * distanceScale;

				float2 densityCP = 0;
				float3 scatterR = 0;
				float3 scatterM = 0;

				float2 localDensity;
				float2 densityPA;

				float2 prevLocalDensity;
				float3 prevLocalInscatterR, prevLocalInscatterM;
				GetAtmosphereDensity(rayStart, planetCenter, lightDir, prevLocalDensity, densityPA);
				ComputeLocalInscattering(prevLocalDensity, densityPA, densityCP, prevLocalInscatterR, prevLocalInscatterM);

				// P - current integration point
				// C - camera position
				// A - top of the atmosphere
				[loop]
				for (float s = 1.0; s < sampleCount; s += 1)
				{
					float3 p = rayStart + step * s;

					GetAtmosphereDensity(p, planetCenter, lightDir, localDensity, densityPA);
					densityCP += (localDensity + prevLocalDensity) * (stepSize / 2.0);

					prevLocalDensity = localDensity;

					float3 localInscatterR, localInscatterM;
					ComputeLocalInscattering(localDensity, densityPA, densityCP, localInscatterR, localInscatterM);

					scatterR += (localInscatterR + prevLocalInscatterR) * (stepSize / 2.0);
					scatterM += (localInscatterM + prevLocalInscatterM) * (stepSize / 2.0);

					prevLocalInscatterR = localInscatterR;
					prevLocalInscatterM = localInscatterM;
				}

				float3 m = scatterM;
				// phase function
				ApplyPhaseFunction(scatterR, scatterM, dot(rayDir, -lightDir.xyz));
				//scatterR = 0;
				float3 lightInscatter = (scatterR * _ScatteringR + scatterM * _ScatteringM) * _IncomingLight.xyz;
				lightInscatter += RenderSun(m, dot(rayDir, -lightDir.xyz)) * _SunIntensity;
				float3 lightExtinction = exp(-(densityCP.x * _ExtinctionR + densityCP.y * _ExtinctionM));

				extinction = float4(lightExtinction, 0);
				return float4(lightInscatter, 1);
			}

			float4 frag (PSInput i) : SV_Target
			{
				float2 uv = i.uv.xy;
				float depth = SAMPLE_DEPTH_TEXTURE(_CameraDepthTexture, uv);
				float linearDepth = Linear01Depth(depth);

				float3 wpos = i.wpos;
				float3 rayStart = _WorldSpaceCameraPos;
				float3 rayDir = wpos - _WorldSpaceCameraPos;
				rayDir *= linearDepth;

				float rayLength = length(rayDir);
				rayDir /= rayLength;

				float3 planetCenter = _WorldSpaceCameraPos;
				planetCenter = float3(0, -_PlanetRadius, 0);
				float2 intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, _PlanetRadius + _AtmosphereHeight);
				if (linearDepth > 0.99999)
				{
					rayLength = 1e20;
				}
				rayLength = min(intersection.y, rayLength);

				intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, _PlanetRadius);
				if (intersection.x > 0)
					rayLength = min(rayLength, intersection.x);

				float4 extinction;
				_SunIntensity = 0;
				float4 inscattering = IntegrateInscattering(rayStart, rayDir, rayLength, planetCenter, _DistanceScale, _LightDir, 16, extinction);

				float shadow = tex2D(_LightShaft1, uv.xy).x;
				shadow = (pow(shadow, 4) + shadow) / 2;
				shadow = max(0.1, shadow);

				//inscattering *= shadow;

				float4 background = tex2D(_Background, uv);

				if (linearDepth > 0.99999)
				{
					background *= shadow;

					inscattering = 0;
					extinction = 1;
				}



				float4 c = background * extinction + inscattering;

				//c = float4(tex2D(_LightShaft1, uv).x - 0.4f, 0, 0, 1);

				return c;
			}
			ENDCG
		}
	}
}


*/
