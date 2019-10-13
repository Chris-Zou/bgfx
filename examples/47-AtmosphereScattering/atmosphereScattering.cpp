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

namespace
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

		static bool isInitialized;
		static bgfx::VertexLayout ms_layout;
	};

	bool ScreenSpaceQuadVertex::isInitialized = false;
	bgfx::VertexLayout ScreenSpaceQuadVertex::ms_layout;

	static void setScreenSpaceQuad(
		const float _textureWidth,
		const float _textureHeight,
		const bool _originBottomLeft = false,
		const float _width = 1.0f,
		const float _height = 1.0f)
	{
		if (3 == bgfx::getAvailTransientVertexBuffer(3, ScreenSpaceQuadVertex::ms_layout))
		{

			bgfx::TransientVertexBuffer vb;
			bgfx::allocTransientVertexBuffer(&vb, 3, ScreenSpaceQuadVertex::ms_layout);
			ScreenSpaceQuadVertex* vertex = (ScreenSpaceQuadVertex*)vb.data;

			const float zz = 0.0f;

			const float minx = -_width;
			const float maxx = _width;
			const float miny = 0.0f;
			const float maxy = _height * 2.0f;

			const float texelHalfW = 0.0f / _textureWidth;
			const float texelHalfH = 0.0f / _textureHeight;
			const float minu = -1.0f + texelHalfW;
			const float maxu = 1.0f + texelHalfW;

			float minv = texelHalfH;
			float maxv = 2.0f + texelHalfH;

			if (_originBottomLeft) {
				float temp = minv;
				minv = maxv;
				maxv = temp;

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

	class ExampleShadowMapping : public entry::AppI
	{
	public:
		ExampleShadowMapping(const char* _name, const char* _description, const char* _url)
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

			m_fMieG = 0.76f;
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

			if (m_bIsFirstFrame)
			{
				compileShaders();
			}

			m_caps = bgfx::getCaps();

			imguiCreate();

			cameraCreate();
			cameraSetPosition({ 0.0f, 2.0f, 0.0f });
			cameraSetHorizontalAngle(bx::kPi / 2.0);

			ScreenSpaceQuadVertex::init();

			m_bIsFirstFrame = false;
		}

		void setConstantUniforms()
		{
			bgfx::setUniform(m_planetRadius, &m_fPlanetRadius);
			bgfx::setUniform(m_atmosphereHeight, &m_fAtmosphereHeight);
			bgfx::setUniform(m_sunIntensity, &m_fSunIntensity);
			bgfx::setUniform(m_distanceScale, &m_fDistanceScale);
			bgfx::setUniform(m_densityScaleHeight, &m_fDensityHeight);

			bgfx::setUniform(m_scatteringR, &m_vRayleighSct);
			bgfx::setUniform(m_scatteringM, &m_vMieSct);
			bgfx::setUniform(m_extinctionR, &m_vExtinctionR);
			bgfx::setUniform(m_extinctionM, &m_vExtinctionM);

			bgfx::setUniform(m_mieG, &m_fMieG);
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

			imguiDestroy();

			return 0;
		}

		bool update() override
		{
			if (entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState)) {
				return false;
			}

			/*imguiBeginFrame(m_mouseState.m_mx
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

			ImGui::End();

			imguiEndFrame();*/

			float proj[16];
			bx::mtxProj(proj, 60.0f, float(m_width) / float(m_height), 0.1f, 1000.0f, bgfx::getCaps()->homogeneousDepth);

			// Update camera
			float view[16];
			cameraGetViewMtx(view);
			bgfx::setViewTransform(0, view, proj);

			bx::Vec3 cameraPos = cameraGetPosition();
			bgfx::setUniform(m_cameraPos, &cameraPos);

			setScreenSpaceQuad(float(m_width), float(m_height), m_caps->originBottomLeft);

			bgfx::submit(0, m_atmophereScattering);

			bgfx::frame();

			return true;
		}

	public:
		entry::MouseState m_mouseState;

		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_debug;
		uint32_t m_reset;

		const bgfx::Caps *m_caps;

		bool m_bIsFirstFrame = true;

		bgfx::ProgramHandle m_atmophereScattering;

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

		bx::Vec3 m_vIncomingLight;
		bx::Vec3 m_vLightDir;
#pragma endregion
	};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	ExampleShadowMapping
	, "43-PBR_IBL"
	, "PBR_IBL."
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
