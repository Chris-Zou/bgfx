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

#include "SceneManager/sceneMangement.h"
#include "SceneManager/lightVolumeShape.h"
#include "Renderer/toneMappingRender.h"
#include "ShaderCompiler/ShaderCompiler.h"
#include "SceneManager/gltf_model_loading.h"

namespace TAA
{
	static float s_texelHalf = 0.0f;

	std::vector<glm::vec3> LIGHT_COLORS = {
		{ 1.0f, 1.0f, 1.0f },
		{ 1.0f, 0.1f, 0.1f },
		{ 0.1f, 1.0f, 0.1f },
		{ 0.1f, 0.1f, 1.0f },
		{ 1.0f, 0.1f, 1.0f },
		{ 1.0f, 1.0f, 0.1f },
		{ 0.1f, 1.0f, 1.0f },
	};

	std::vector<glm::vec3> sampleUnitCylinderUniformly(size_t N) {
		std::default_random_engine generator(10);
		std::uniform_real_distribution<float> rand(0.0f, 1.0f);
		std::vector<glm::vec3> output(N);
		for (size_t i = 0; i < N; ++i) {
			output[i] = glm::vec3{
				sqrt(rand(generator)),
				rand(generator) * 2 * bx::kPi,
				rand(generator),
			};
		}
		return output;
	}

	class LightSet {
	public:
		size_t numActiveLights;
		size_t maxNumLights = 2048;
		Dolphin::Mesh volumeMesh;
		std::vector<glm::vec3> initialPositions;
		std::vector<glm::vec4> positionRadiusData;
		std::vector<glm::vec4> colorIntensityData;

		void init()
		{
			Dolphin::lightVolumeShape factory{ 2 };
			volumeMesh = factory.getMesh();
			initialPositions = sampleUnitCylinderUniformly(maxNumLights);
			positionRadiusData.resize(maxNumLights);
			colorIntensityData.resize(maxNumLights);
			size_t numColors = LIGHT_COLORS.size();
			// Initialize color data for all lights
			for (size_t i = 0; i < maxNumLights; i++) {
				colorIntensityData[i] = glm::vec4{ LIGHT_COLORS[i % numColors], 1.0f };
			}
		}

		void destroy()
		{
			Dolphin::destroy(volumeMesh);
		}
	};

	struct PBRShaderUniforms {
		bgfx::UniformHandle s_baseColor = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_normal = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_metallicRoughness = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_emissive = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_occlusion = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_factors = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_normalTransform = BGFX_INVALID_HANDLE;
	};

	void init(PBRShaderUniforms& uniforms)
	{
		uniforms.s_baseColor = bgfx::createUniform("s_baseColor", bgfx::UniformType::Sampler);
		uniforms.s_normal = bgfx::createUniform("s_normal", bgfx::UniformType::Sampler);
		uniforms.s_metallicRoughness = bgfx::createUniform("s_metallicRoughness", bgfx::UniformType::Sampler);
		uniforms.s_emissive = bgfx::createUniform("s_emissive", bgfx::UniformType::Sampler);
		uniforms.s_occlusion = bgfx::createUniform("s_occlusion", bgfx::UniformType::Sampler);
		// pack baseColorFactor, emissiveFactor, roughnessFactor and metallicFactor into this uniform
		uniforms.u_factors = bgfx::createUniform("u_factors", bgfx::UniformType::Vec4, 3);
		uniforms.u_normalTransform = bgfx::createUniform("u_normalTransform", bgfx::UniformType::Mat4);
	}

	void destroy(PBRShaderUniforms& uniforms)
	{
		bgfx::destroy(uniforms.s_baseColor);
		bgfx::destroy(uniforms.s_normal);
		bgfx::destroy(uniforms.s_metallicRoughness);
		bgfx::destroy(uniforms.s_emissive);
		bgfx::destroy(uniforms.s_occlusion);
		bgfx::destroy(uniforms.u_factors);
		bgfx::destroy(uniforms.u_normalTransform);
	}

	void bindUniforms(const PBRShaderUniforms& uniforms, const Dolphin::PBRMaterial& material, const glm::mat4& transform)
	{
		bgfx::setTexture(0, uniforms.s_baseColor, material.baseColorTexture);
		bgfx::setTexture(1, uniforms.s_normal, material.normalTexture);
		bgfx::setTexture(2, uniforms.s_metallicRoughness, material.metallicRoughnessTexture);
		bgfx::setTexture(3, uniforms.s_emissive, material.emissiveTexture);
		bgfx::setTexture(4, uniforms.s_occlusion, material.occlusionTexture);
		// pack baseColorFactor, emissiveFactor, roughnessFactor
		// and metallicFactor into this uniform
		bgfx::setUniform(uniforms.u_factors, &material.baseColorFactor, 3);

		// Transforms
		bgfx::setTransform(glm::value_ptr(transform));
		glm::mat4 normalTransform{ glm::transpose(glm::inverse(transform)) };
		bgfx::setUniform(uniforms.u_normalTransform, glm::value_ptr(normalTransform));
	}

	struct DeferredSceneUniforms
	{
		bgfx::UniformHandle s_baseColorRoughness = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_normalMetallic = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_emissiveOcclusion = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_depth = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_cameraPos = BGFX_INVALID_HANDLE;
	};

	void init(DeferredSceneUniforms& uniforms)
	{
		uniforms.s_baseColorRoughness = bgfx::createUniform("s_baseColorRoughness", bgfx::UniformType::Sampler);
		uniforms.s_normalMetallic = bgfx::createUniform("s_normalMetallic", bgfx::UniformType::Sampler);
		uniforms.s_emissiveOcclusion = bgfx::createUniform("s_emissiveOcclusion", bgfx::UniformType::Sampler);
		uniforms.s_depth = bgfx::createUniform("s_depth", bgfx::UniformType::Sampler);
		uniforms.u_cameraPos = bgfx::createUniform("u_cameraPos", bgfx::UniformType::Vec4);
	}

	void destroy(DeferredSceneUniforms& uniforms)
	{
		bgfx::destroy(uniforms.s_baseColorRoughness);
		bgfx::destroy(uniforms.s_normalMetallic);
		bgfx::destroy(uniforms.s_emissiveOcclusion);
		bgfx::destroy(uniforms.s_depth);
		bgfx::destroy(uniforms.u_cameraPos);
	}

	struct PointLightUniforms
	{
		bgfx::UniformHandle u_lightColorIntensity = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_lightPosRadius = BGFX_INVALID_HANDLE;
	};

	void init(PointLightUniforms& uniforms)
	{
		uniforms.u_lightColorIntensity = bgfx::createUniform("u_lightColorIntensity", bgfx::UniformType::Vec4);
		uniforms.u_lightPosRadius = bgfx::createUniform("u_lightPosRadius", bgfx::UniformType::Vec4);
	}

	void destroy(PointLightUniforms& uniforms)
	{
		bgfx::destroy(uniforms.u_lightColorIntensity);
		bgfx::destroy(uniforms.u_lightPosRadius);
	}

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

	class ExampleTAA : public entry::AppI
	{
	public:
		ExampleTAA(const char* _name, const char* _description, const char* _url)
			: entry::AppI(_name, _description, _url)
		{
		}

		void compileNeededShaders()
		{
			std::string prefix("../49-taa/");

			m_writeToRTProgram = compileSingleGraphicsProgram(prefix, "vs_deferred_pbr", "fs_deferred_pbr");
			m_lightStencilProgram = compileSingleGraphicsProgram(prefix, "vs_light_stencil", "fs_light_stencil");
			m_pointLightVolumeProgram = compileSingleGraphicsProgram(prefix, "vs_point_light_volume", "fs_point_light_volume");
			m_emissivePassProgram = compileSingleGraphicsProgram(prefix, "vs_emissive_pass", "fs_emissive_pass");

			m_copyHistoryBufferProgram = compileSingleGraphicsProgram(prefix, "vs_fullscreen", "fs_copy_buffer");

			m_velocityBufferProgram = compileSingleGraphicsProgram(prefix, "vs_blit", "fs_velocity_prepass");

			m_taaProgram = compileSingleGraphicsProgram(prefix, "vs_taa", "fs_taa");
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

			m_caps = bgfx::getCaps();
			m_isComputeSupported = !!(m_caps->supported & BGFX_CAPS_COMPUTE);

			if (!m_isComputeSupported)
			{
				return;
			}

			compileNeededShaders();

			TAA::init(m_pbrUniforms);
			TAA::init(m_deferredSceneUniforms);
			TAA::init(m_pointLightUniforms);

			u_historyBufferHandle = bgfx::createUniform("s_historyBuffer", bgfx::UniformType::Sampler);

			u_depthBufferHandle = bgfx::createUniform("s_depthBuffer", bgfx::UniformType::Sampler);

			u_mainTexBufferHandle = bgfx::createUniform("s_mainTex", bgfx::UniformType::Sampler);
			u_velocityBufferHandle = bgfx::createUniform("s_velocityBuffer", bgfx::UniformType::Sampler);
			u_prevBufferHandle = bgfx::createUniform("s_prevBuffer", bgfx::UniformType::Sampler);

			u_prevVHandle = bgfx::createUniform("u_prevV", bgfx::UniformType::Mat4);
			u_prevPHandle = bgfx::createUniform("u_prevP", bgfx::UniformType::Mat4);
			u_invCurrVHandle = bgfx::createUniform("u_invCurrV", bgfx::UniformType::Mat4);
			u_invCurrPHandle = bgfx::createUniform("u_invCurrP", bgfx::UniformType::Mat4);

			u_params = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);
			u_texelSize = bgfx::createUniform("texelSize", bgfx::UniformType::Vec4);
			u_timeMotionScale = bgfx::createUniform("sinTimeMotionScale", bgfx::UniformType::Vec4);
			u_jitterUV = bgfx::createUniform("jitterUV", bgfx::UniformType::Vec4);

			m_model = Dolphin::loadGltfModel("meshes/Sponza/", "Sponza.gltf");

			m_lightSet.init();
			m_lightSet.numActiveLights = 256;
			m_totalBirghtness = 100.0f;

			m_toneMapParams.m_width = m_width;
			m_toneMapParams.m_height = m_height;
			m_toneMapParams.m_originBottomLeft = m_caps->originBottomLeft;
			m_toneMapPass.init(m_caps);

			imguiCreate();

			s_texelHalf = bgfx::RendererType::Direct3D9 == m_caps->rendererType ? 0.5f : 0.0f;

			cameraCreate();
			cameraSetPosition({ 0.0f, 2.0f, 0.0f });
			cameraSetHorizontalAngle(bx::kPi / 2.0);

			m_oldWidth = 0;
			m_oldHeight = 0;
			m_oldReset = m_reset;

			m_time = 0.0f;

			initHaltonPattern();
		}

		void setMotionBlurUniforms(uint16_t _width, uint16_t _height)
		{
			bgfx::setUniform(u_prevVHandle, m_prevV);
			bgfx::setUniform(u_prevPHandle, m_prevP);
			bgfx::setUniform(u_invCurrVHandle, m_invCurrV);
			bgfx::setUniform(u_invCurrPHandle, m_invCurrP);
		}

		void setTaaUniforms(uint16_t _width, uint16_t _height)
		{
			float tmpJitterSample[4];
			tmpJitterSample[0] = m_activeSample[0] / _width;
			tmpJitterSample[1] = m_activeSample[1] / _height;
			tmpJitterSample[2] = m_activeSample[2] / _width;
			tmpJitterSample[3] = m_activeSample[3] / _height;
			bgfx::setUniform(u_jitterUV, &tmpJitterSample[0]);

			float params[4];
			params[0] = m_nearPlane;
			params[1] = m_farPlane;
			params[2] = 0.88f;
			params[3] = 0.97f;
			bgfx::setUniform(u_params, params);

			float sintime[4];
			sintime[0] = 0.0f;
			sintime[1] = 0.0f;
			sintime[2] = 1.0f;
			sintime[3] = 0.0f;
			bgfx::setUniform(u_timeMotionScale, sintime);

			float texelSize[4];
			texelSize[0] = 1.0f / (float)_width;
			texelSize[1] = 1.0f / (float)_height;
			texelSize[2] = (float)_width;
			texelSize[3] = (float)_height;

			bgfx::setUniform(u_texelSize, texelSize);
		}

		float haltonSequence(int prime, int index = 1)
		{
			float r = 0.0f;
			float f = 1.0f;
			int i = index;

			while (i > 0)
			{
				f /= prime;
				r += f * (i % prime);
				i = (int)glm::floor(i / (float)prime);
			}

			return r;
		}

		void initHaltonPattern()
		{
			for (int i = 0, n = 32 / 2; i != n; ++i)
			{
				float u = haltonSequence(2, i + 1) - 0.5f;
				float v = haltonSequence(3, i + 1) - 0.5f;
				m_halton_pattern[2 * i + 0] = u;
				m_halton_pattern[2 * i + 1] = v;
			}
		}

		glm::vec2 sampleHaltonSequence(int index)
		{
			int n = 32 / 2;
			int i = index % n;

			float x = m_halton_pattern[2 * i + 0];
			float y = m_halton_pattern[2 * i + 1];

			return glm::vec2(x, y);
		}

		void updateJitterData()
		{
			if (m_activeIndex == -2)
			{
				m_activeSample = glm::vec4();
				m_activeIndex += 1;
			}
			else
			{
				m_activeIndex += 1;
				m_activeIndex %= (32 / 2);


				glm::vec2 sample = sampleHaltonSequence(m_activeIndex);
				m_activeSample.z = m_activeSample.x;
				m_activeSample.w = m_activeSample.y;
				m_activeSample.x = sample.x;
				m_activeSample.y = sample.y;
			}
		}

		virtual int shutdown() override
		{
			if (m_isComputeSupported)
			{
				if (bgfx::isValid(m_hdrFrameBuffer))
				{
					bgfx::destroy(m_hdrFrameBuffer);
				}

				if (bgfx::isValid(m_gBuffer))
				{
					bgfx::destroy(m_gBuffer);
				}

				if (bgfx::isValid(m_lightGBuffer))
				{
					bgfx::destroy(m_lightGBuffer);
				}

				m_toneMapPass.destroy();

				destroy(m_pbrUniforms);
				destroy(m_deferredSceneUniforms);
				destroy(m_pointLightUniforms);
				bgfx::destroy(m_writeToRTProgram);
				bgfx::destroy(m_lightStencilProgram);
				bgfx::destroy(m_pointLightVolumeProgram);
				bgfx::destroy(m_emissivePassProgram);
				destroy(m_model);
				m_lightSet.destroy();

				if (bgfx::isValid(m_copyHistoryBufferProgram))
					bgfx::destroy(m_copyHistoryBufferProgram);

				if(bgfx::isValid(m_velocityBufferProgram))
					bgfx::destroy(m_velocityBufferProgram);

				if (bgfx::isValid(m_taaProgram))
					bgfx::destroy(m_taaProgram);

				if (bgfx::isValid(m_copyHistFrameBuffer))
					bgfx::destroy(m_copyHistFrameBuffer);

				if (bgfx::isValid(m_motionBlurFrameBuffer))
					bgfx::destroy(m_motionBlurFrameBuffer);

				if (bgfx::isValid(m_taaFrameBuffer))
					bgfx::destroy(m_taaFrameBuffer);

				if (bgfx::isValid(u_historyBufferHandle))
					bgfx::destroy(u_historyBufferHandle);

				if (bgfx::isValid(u_depthBufferHandle))
					bgfx::destroy(u_depthBufferHandle);

				if (bgfx::isValid(u_prevVPHandle))
					bgfx::destroy(u_prevVPHandle);

				if (bgfx::isValid(u_prevVHandle))
					bgfx::destroy(u_prevVHandle);

				if (bgfx::isValid(u_prevPHandle))
					bgfx::destroy(u_prevPHandle);

				if (bgfx::isValid(u_invCurrVHandle))
					bgfx::destroy(u_invCurrVHandle);

				if (bgfx::isValid(u_invCurrPHandle))
					bgfx::destroy(u_invCurrPHandle);

				if (bgfx::isValid(u_mainTexBufferHandle))
					bgfx::destroy(u_mainTexBufferHandle);

				if (bgfx::isValid(u_velocityBufferHandle))
					bgfx::destroy(u_velocityBufferHandle);

				if (bgfx::isValid(u_prevBufferHandle))
					bgfx::destroy(u_prevBufferHandle);

				cameraDestroy();

				imguiDestroy();
			}

			bgfx::shutdown();

			return 0;
		}

		void initializeFrameBuffers()
		{
			m_oldWidth = m_width;
			m_oldHeight = m_height;
			m_oldReset = m_reset;

			uint32_t msaa = (m_reset & BGFX_RESET_MSAA_MASK) >> BGFX_RESET_MSAA_SHIFT;

			if (bgfx::isValid(m_hdrFrameBuffer))
			{
				bgfx::destroy(m_hdrFrameBuffer);
			}

			if (bgfx::isValid(m_gBuffer))
			{
				bgfx::destroy(m_gBuffer);
				bgfx::destroy(m_lightGBuffer);
				m_gbufferTex[0].idx = bgfx::kInvalidHandle;
				m_gbufferTex[1].idx = bgfx::kInvalidHandle;
				m_gbufferTex[2].idx = bgfx::kInvalidHandle;
				m_gbufferTex[3].idx = bgfx::kInvalidHandle;
				m_gbufferTex[4].idx = bgfx::kInvalidHandle;
				m_gbufferTex[5].idx = bgfx::kInvalidHandle;
			}

			const uint64_t tsFlags = 0
				| BGFX_SAMPLER_MIN_POINT
				| BGFX_SAMPLER_MAG_POINT
				| BGFX_SAMPLER_MIP_POINT
				| BGFX_SAMPLER_U_CLAMP
				| BGFX_SAMPLER_V_CLAMP
				;

			bgfx::Attachment gbufferAt[6] = {};

			// The gBuffers store the following, in order:
			// - RGB - Base Color, A -Roughness
			// - RGB16 - Normal, A - Metalness
			// - RGB - Emissive, A - Occlusion
			// - R32F - Depth
			// - RGBA16F - Final Radiance
			m_gbufferTex[0] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT | tsFlags);
			m_gbufferTex[1] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | tsFlags);
			m_gbufferTex[2] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT | tsFlags);
			m_gbufferTex[3] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::R32F, BGFX_TEXTURE_RT | tsFlags);
			m_gbufferTex[4] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | tsFlags);

			bgfx::setName(m_gbufferTex[0], "Albedo & Roughness");
			bgfx::setName(m_gbufferTex[1], "Normal & Metalness");
			bgfx::setName(m_gbufferTex[2], "Emissive & Occlusion");
			bgfx::setName(m_gbufferTex[3], "Depth");
			bgfx::setName(m_gbufferTex[4], "Final Radiance");

			gbufferAt[0].init(m_gbufferTex[0]);
			gbufferAt[1].init(m_gbufferTex[1]);
			gbufferAt[2].init(m_gbufferTex[2]);
			gbufferAt[3].init(m_gbufferTex[3]);
			gbufferAt[4].init(m_gbufferTex[4]);



			m_gbufferTex[5] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT | tsFlags);
			gbufferAt[5].init(m_gbufferTex[5]);

			bgfx::setName(m_gbufferTex[5], "Depth Stencil");

			m_gBuffer = bgfx::createFrameBuffer(BX_COUNTOF(gbufferAt), gbufferAt, true);
			m_lightGBuffer = bgfx::createFrameBuffer(2, &gbufferAt[4], false);

			

			m_toneMapParams.m_width = m_width;
			m_toneMapParams.m_height = m_height;

			m_hdrFbTextures[0] = bgfx::createTexture2D(
				uint16_t(m_width)
				, uint16_t(m_height)
				, false
				, 1
				, bgfx::TextureFormat::RGBA16F
				, (uint64_t(msaa + 1) << BGFX_TEXTURE_RT_MSAA_SHIFT) | BGFX_SAMPLER_UVW_CLAMP | BGFX_SAMPLER_POINT
			);

			const uint64_t textureFlags = BGFX_TEXTURE_RT_WRITE_ONLY | (uint64_t(msaa + 1) << BGFX_TEXTURE_RT_MSAA_SHIFT);

			bgfx::TextureFormat::Enum depthFormat;
			if (bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D24S8, textureFlags)) {
				depthFormat = bgfx::TextureFormat::D24S8;
			}
			else {
				depthFormat = bgfx::TextureFormat::D32;
			}

			m_hdrFbTextures[1] = bgfx::createTexture2D(
				uint16_t(m_width)
				, uint16_t(m_height)
				, false
				, 1
				, depthFormat
				, textureFlags
			);

			bgfx::setName(m_hdrFbTextures[0], "HDR Buffer");

			m_hdrFrameBuffer = bgfx::createFrameBuffer(BX_COUNTOF(m_hdrFbTextures), m_hdrFbTextures, true);

			m_historyRT[0] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | tsFlags);
			m_historyRT[1] = bgfx::createTexture2D(
				uint16_t(m_width)
				, uint16_t(m_height)
				, false
				, 1
				, depthFormat
				, textureFlags
			);
			m_copyHistFrameBuffer = bgfx::createFrameBuffer(BX_COUNTOF(m_historyRT), m_historyRT, false);
			bgfx::setName(m_historyRT[0], "Copy Buffer");

			m_motionBlurRT[0] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | tsFlags);
			m_motionBlurRT[1] = bgfx::createTexture2D(
				uint16_t(m_width)
				, uint16_t(m_height)
				, false
				, 1
				, depthFormat
				, textureFlags
			);
			m_motionBlurFrameBuffer = bgfx::createFrameBuffer(BX_COUNTOF(m_motionBlurRT), m_motionBlurRT, false);
			bgfx::setName(m_motionBlurRT[0], "MotionBlur Buffer");

			m_taaRT[0] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | tsFlags);
			m_taaRT[1] = bgfx::createTexture2D(
				uint16_t(m_width)
				, uint16_t(m_height)
				, false
				, 1
				, depthFormat
				, textureFlags
			);
			m_taaFrameBuffer = bgfx::createFrameBuffer(BX_COUNTOF(m_taaRT), m_taaRT, false);
			bgfx::setName(m_taaRT[0], "TAA Buffer");

			m_reprojectionRT[0] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | tsFlags);
			m_reprojectionRT[1] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | tsFlags);
		}

		bool update() override
		{
			if (entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState)) {
				return false;
			}

			if (!m_isComputeSupported) {
				return false;
			}

			if (!bgfx::isValid(m_hdrFrameBuffer)
				|| m_oldWidth != m_width
				|| m_oldHeight != m_height
				|| m_oldReset != m_reset) {
				initializeFrameBuffers();
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

			int numActiveLights = int32_t(m_lightSet.numActiveLights);
			ImGui::SliderInt("Num lights", &numActiveLights, 1, int(m_lightSet.maxNumLights));
			ImGui::DragFloat("Total Brightness", &m_totalBirghtness, 0.5f, 0.0f, 250.0f);
			ImGui::Checkbox("UseTAA", &m_bUseTAA);

			ImGui::End();

			imguiEndFrame();

			// update camera jitter data
			updateJitterData();

			bgfx::ViewId meshPass = 0;
			bgfx::setViewRect(meshPass, 0, 0, uint16_t(m_width), uint16_t(m_height));
			bgfx::setViewFrameBuffer(meshPass, m_gBuffer);
			bgfx::setViewName(meshPass, "Draw Meshes");

			bgfx::ViewId lightingPass = 1;
			bgfx::setViewFrameBuffer(lightingPass, m_lightGBuffer);
			bgfx::setViewRect(lightingPass, 0, 0, uint16_t(m_width), uint16_t(m_height));

			bgfx::setViewMode(lightingPass, bgfx::ViewMode::Sequential);
			bgfx::setViewName(lightingPass, "Lighting Pass");

			bgfx::ViewId emissivePass = 2;
			bgfx::setViewFrameBuffer(emissivePass, m_lightGBuffer);
			bgfx::setViewRect(emissivePass, 0, 0, uint16_t(m_width), uint16_t(m_height));
			bgfx::setViewName(emissivePass, "Emissive Pass");

			bgfx::touch(meshPass);

			int64_t now = bx::getHPCounter();
			static int64_t last = now;
			const int64_t frameTime = now - last;
			last = now;
			const double freq = double(bx::getHPFrequency());
			const float deltaTime = (float)(frameTime / freq);
			//m_time += deltaTime;

			float proj[16];
			bx::mtxProj(proj, 60.0f, float(m_width) / float(m_height), m_nearPlane, m_farPlane, bgfx::getCaps()->homogeneousDepth);

			float view[16];

			cameraUpdate(0.1f * deltaTime, m_mouseState);
			cameraGetViewMtx(view);
			if (m_isFirstFrame)
			{
				memcpy(m_prevV, view, sizeof(view));
				memcpy(m_prevP, proj, sizeof(proj));
				
				m_isFirstFrame = false;
			}

			bx::mtxInverse(m_invCurrP, proj);
			bx::mtxInverse(m_invCurrV, view);

			bgfx::setViewTransform(meshPass, view, proj);

			glm::mat4 mtx = glm::identity<glm::mat4>();

			uint64_t stateOpaque = 0
				| BGFX_STATE_WRITE_RGB
				| BGFX_STATE_WRITE_A
				| BGFX_STATE_WRITE_Z
				| BGFX_STATE_DEPTH_TEST_LESS
				| BGFX_STATE_CULL_CCW;

			bx::Vec3 cameraPos = cameraGetPosition();
			bgfx::setUniform(m_deferredSceneUniforms.u_cameraPos, &cameraPos.x);

			const Dolphin::MeshGroup& meshes = m_model.opaqueMeshes;
			for (size_t i = 0; i < meshes.meshes.size(); ++i) {
				const auto& mesh = meshes.meshes[i];
				const auto& transform = meshes.transforms[i];
				const auto& material = meshes.materials[i];

				bgfx::setState(stateOpaque);
				bindUniforms(m_pbrUniforms, material, transform);
				mesh.setBuffers();
				bgfx::submit(meshPass, m_writeToRTProgram);
			}

			{
				m_lightSet.numActiveLights = uint16_t(numActiveLights);

				constexpr float sceneWidth = 12.0f;
				constexpr float sceneLength = 4.0f;
				constexpr float sceneHeight = 10.0f;
				constexpr float timeCoeff = 0.3f;

				float N = float(m_lightSet.numActiveLights);
				float intensity = m_totalBirghtness / N;
				constexpr float EPSILON = 0.01f;
				float radius = bx::sqrt(intensity / EPSILON);

				for (size_t i = 0; i < m_lightSet.numActiveLights; ++i) {
					glm::vec3& initial = m_lightSet.initialPositions[i];
					float r = initial.x;
					float phaseOffset = initial.y;
					float z = initial.z;
					m_lightSet.positionRadiusData[i].x = r * sceneWidth * bx::cos(timeCoeff * m_time + phaseOffset);
					m_lightSet.positionRadiusData[i].z = r * sceneLength * bx::sin(timeCoeff * m_time + phaseOffset);
					m_lightSet.positionRadiusData[i].y = sceneHeight * z;
					m_lightSet.colorIntensityData[i].w = intensity;
					m_lightSet.positionRadiusData[i].w = radius;
				}
			}

			bgfx::setViewTransform(lightingPass, view, proj);

			uint64_t stencilState = 0
				| BGFX_STATE_DEPTH_TEST_LESS;

			for (size_t i = 0; i < m_lightSet.numActiveLights; ++i) {
				uint32_t frontStencilFunc = BGFX_STENCIL_TEST_ALWAYS
					| BGFX_STENCIL_FUNC_REF(0)
					| BGFX_STENCIL_FUNC_RMASK(0xFF)
					| BGFX_STENCIL_OP_FAIL_S_KEEP
					| BGFX_STENCIL_OP_FAIL_Z_INCR
					| BGFX_STENCIL_OP_PASS_Z_KEEP;
				uint32_t backStencilFunc = BGFX_STENCIL_TEST_ALWAYS
					| BGFX_STENCIL_FUNC_REF(0)
					| BGFX_STENCIL_FUNC_RMASK(0xFF)
					| BGFX_STENCIL_OP_FAIL_S_KEEP
					| BGFX_STENCIL_OP_FAIL_Z_KEEP
					| BGFX_STENCIL_OP_PASS_Z_INCR;

				glm::mat4 modelTransform = glm::identity<glm::mat4>();
				glm::vec3 position{ m_lightSet.positionRadiusData[i].x, m_lightSet.positionRadiusData[i].y, m_lightSet.positionRadiusData[i].z };
				glm::vec3 scale{ m_lightSet.positionRadiusData[i].w };
				modelTransform = glm::scale(
					glm::translate(modelTransform, position),
					scale
				);

				bgfx::setTransform(glm::value_ptr(modelTransform));
				bgfx::setState(stencilState);
				bgfx::setStencil(frontStencilFunc, backStencilFunc);
				m_lightSet.volumeMesh.setBuffers();
				bgfx::submit(lightingPass, m_lightStencilProgram);

				uint64_t lightVolumeState = 0
					| BGFX_STATE_WRITE_RGB
					| BGFX_STATE_CULL_CW
					| BGFX_STATE_BLEND_ADD;
				frontStencilFunc = BGFX_STENCIL_TEST_EQUAL
					| BGFX_STENCIL_FUNC_RMASK(0xFF)
					| BGFX_STENCIL_FUNC_REF(0);
				backStencilFunc = BGFX_STENCIL_TEST_EQUAL
					| BGFX_STENCIL_FUNC_RMASK(0xFF)
					| BGFX_STENCIL_FUNC_REF(0)
					| BGFX_STENCIL_OP_FAIL_S_REPLACE;

				bgfx::setTransform(glm::value_ptr(modelTransform));
				bgfx::setState(lightVolumeState);
				bgfx::setStencil(frontStencilFunc, backStencilFunc);
				m_lightSet.volumeMesh.setBuffers();
				bgfx::setTexture(0, m_deferredSceneUniforms.s_baseColorRoughness, m_gbufferTex[0], BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
				bgfx::setTexture(1, m_deferredSceneUniforms.s_normalMetallic, m_gbufferTex[1], BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
				bgfx::setTexture(2, m_deferredSceneUniforms.s_emissiveOcclusion, m_gbufferTex[2], BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
				bgfx::setTexture(3, m_deferredSceneUniforms.s_depth, m_gbufferTex[3], BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
				bgfx::setUniform(m_pointLightUniforms.u_lightPosRadius, glm::value_ptr(m_lightSet.positionRadiusData[i]));
				bgfx::setUniform(m_pointLightUniforms.u_lightColorIntensity, glm::value_ptr(m_lightSet.colorIntensityData[i]));
				bgfx::submit(lightingPass, m_pointLightVolumeProgram);
			}

			float orthoProjection[16];
			bx::mtxOrtho(orthoProjection, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 0.0f, m_caps->homogeneousDepth);
			uint64_t emissivePassState = 0
				| BGFX_STATE_WRITE_RGB
				| BGFX_STATE_BLEND_ADD;
			bgfx::setState(emissivePassState);
			Dolphin::ToneMapping::setScreenSpaceQuad(float(m_width), float(m_height), m_caps->originBottomLeft);
			bgfx::setViewTransform(emissivePass, nullptr, orthoProjection);
			bgfx::setTexture(0, m_deferredSceneUniforms.s_emissiveOcclusion, m_gbufferTex[2], BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
			bgfx::submit(emissivePass, m_emissivePassProgram);

			bgfx::ViewId copyPass = emissivePass + 1;
			if (m_bUseTAA)
			{
				if (m_reprojectionRTIndex == -1) //bootstrap
				{
					m_reprojectionRTIndex = 0;
					bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_ALWAYS | BGFX_STATE_CULL_CCW);
					Dolphin::ToneMapping::setScreenSpaceQuad(float(m_width), float(m_height), m_caps->originBottomLeft);
					bgfx::setViewTransform(copyPass, nullptr, orthoProjection);
					bgfx::setTexture(0, u_historyBufferHandle, m_gbufferTex[4], BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
					bgfx::setViewName(copyPass, "Copy Framebuffer");
					bgfx::setViewRect(copyPass, 0, 0, uint16_t(m_width), uint16_t(m_height));

					if (bgfx::isValid(m_copyHistFrameBuffer))
					{
						bgfx::destroy(m_copyHistFrameBuffer);
					}

					bgfx::Attachment copyPassRT[2];
					copyPassRT[0].init(m_reprojectionRT[m_reprojectionRTIndex]);
					copyPassRT[1].init(m_historyRT[1]);
					m_copyHistFrameBuffer = bgfx::createFrameBuffer(BX_COUNTOF(copyPassRT), copyPassRT, false);
					bgfx::setViewFrameBuffer(copyPass, m_copyHistFrameBuffer);
					bgfx::submit(copyPass, m_copyHistoryBufferProgram);
				}
			}
			else
			{
				copyPass = emissivePass;
			}
			
			bgfx::ViewId motionVectorPass = copyPass + 1;
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_ALWAYS | BGFX_STATE_CULL_CCW);
			Dolphin::ToneMapping::setScreenSpaceQuad(float(m_width), float(m_height), m_caps->originBottomLeft);
			bgfx::setViewTransform(motionVectorPass, nullptr, orthoProjection);
			bgfx::setTexture(0, u_depthBufferHandle, m_gbufferTex[3], BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
			bgfx::setViewName(motionVectorPass, "Motion Blur");
			bgfx::setViewRect(motionVectorPass, 0, 0, uint16_t(m_width), uint16_t(m_height));
			bgfx::setViewFrameBuffer(motionVectorPass, m_motionBlurFrameBuffer);
			setMotionBlurUniforms(uint16_t(m_width), uint16_t(m_height));
			bgfx::submit(motionVectorPass, m_velocityBufferProgram);

			if (m_bUseTAA)
			{
				bgfx::ViewId taaPass = motionVectorPass + 1;
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_ALWAYS | BGFX_STATE_CULL_CCW);
				Dolphin::ToneMapping::setScreenSpaceQuad(float(m_width), float(m_height), m_caps->originBottomLeft);
				bgfx::setViewTransform(taaPass, nullptr, orthoProjection);
				bgfx::setTexture(0, u_mainTexBufferHandle, m_gbufferTex[4], BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
				bgfx::setTexture(1, u_velocityBufferHandle, m_motionBlurRT[0], BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
				bgfx::setTexture(2, u_prevBufferHandle, m_historyRT[0], BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
				bgfx::setTexture(3, u_depthBufferHandle, m_gbufferTex[3], BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
				bgfx::setViewName(taaPass, "Temporal Anti Aliasing");
				bgfx::setViewRect(taaPass, 0, 0, uint16_t(m_width), uint16_t(m_height));

				if (bgfx::isValid(m_taaFrameBuffer))
				{
					bgfx::destroy(m_taaFrameBuffer);
				}

				bgfx::Attachment taaAttachment[2];
				taaAttachment[0].init(m_taaRT[0]);
				taaAttachment[1].init(m_reprojectionRT[(m_reprojectionRTIndex + 1) % 2]);
				m_taaFrameBuffer = bgfx::createFrameBuffer(BX_COUNTOF(taaAttachment), taaAttachment, false);

				bgfx::setViewFrameBuffer(taaPass, m_taaFrameBuffer);
				setTaaUniforms(uint16_t(m_width), uint16_t(m_height));
				bgfx::submit(taaPass, m_taaProgram);

				m_toneMapPass.render(m_taaRT[0], m_toneMapParams, deltaTime, taaPass + 1);

				m_reprojectionRTIndex = (m_reprojectionRTIndex + 1) % 2;
			}
			else
			{
				m_toneMapPass.render(m_gbufferTex[4], m_toneMapParams, deltaTime, motionVectorPass + 1);
			}

			bgfx::frame();

			if (!m_isFirstFrame)
			{
				memcpy(m_prevV, view, sizeof(view));
				memcpy(m_prevP, proj, sizeof(proj));
			}

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

		bgfx::ProgramHandle m_writeToRTProgram;
		bgfx::ProgramHandle m_lightStencilProgram;
		bgfx::ProgramHandle m_pointLightVolumeProgram;
		bgfx::ProgramHandle m_emissivePassProgram;
		bgfx::ProgramHandle m_copyHistoryBufferProgram;
		bgfx::ProgramHandle m_velocityBufferProgram;
		bgfx::ProgramHandle m_taaProgram;

		Dolphin::Model m_model;
		PBRShaderUniforms m_pbrUniforms;
		DeferredSceneUniforms m_deferredSceneUniforms;
		PointLightUniforms m_pointLightUniforms;

		LightSet m_lightSet;
		float m_totalBirghtness = 1.0f;

		bool m_bUseTAA = false;

		bgfx::FrameBufferHandle m_gBuffer = BGFX_INVALID_HANDLE;
		bgfx::FrameBufferHandle m_lightGBuffer = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle m_gbufferTex[6];

		bgfx::TextureHandle m_hdrFbTextures[2];
		bgfx::FrameBufferHandle m_hdrFrameBuffer = BGFX_INVALID_HANDLE;

		bgfx::TextureHandle m_historyRT[2];
		bgfx::FrameBufferHandle m_copyHistFrameBuffer = BGFX_INVALID_HANDLE;

		bgfx::TextureHandle m_motionBlurRT[2];
		bgfx::FrameBufferHandle m_motionBlurFrameBuffer = BGFX_INVALID_HANDLE;

		bgfx::TextureHandle m_taaRT[2];
		bgfx::FrameBufferHandle m_taaFrameBuffer = BGFX_INVALID_HANDLE;

		bgfx::TextureHandle m_reprojectionRT[2];
		bgfx::FrameBufferHandle m_reprojectionFrameBuffer = BGFX_INVALID_HANDLE;
		int m_reprojectionRTIndex = -1;

		bgfx::UniformHandle u_historyBufferHandle = BGFX_INVALID_HANDLE;

		bgfx::UniformHandle u_depthBufferHandle = BGFX_INVALID_HANDLE;

		bgfx::UniformHandle u_prevVPHandle = BGFX_INVALID_HANDLE;

		bgfx::UniformHandle u_prevVHandle = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_prevPHandle = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_invCurrVHandle = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_invCurrPHandle = BGFX_INVALID_HANDLE;

		bgfx::UniformHandle u_mainTexBufferHandle = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_velocityBufferHandle = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_prevBufferHandle = BGFX_INVALID_HANDLE;
		
		Dolphin::ToneMapParams m_toneMapParams;
		Dolphin::ToneMapping m_toneMapPass;

		const bgfx::Caps* m_caps;
		float m_time;

		bool m_isComputeSupported = true;

		float m_nearPlane = 0.1f;
		float m_farPlane = 1000.0f;

		float m_prevV[16];
		float m_prevP[16];
		float m_invCurrV[16];
		float m_invCurrP[16];

		bool m_isFirstFrame = true;

		bgfx::UniformHandle u_params;
		bgfx::UniformHandle u_texelSize;
		bgfx::UniformHandle u_jitterUV;
		bgfx::UniformHandle u_timeMotionScale;

		//unjitter data
		glm::vec4 m_activeSample;
		int m_activeIndex = -2;
		float m_halton_pattern[32];
	};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	TAA::ExampleTAA
	, "49-Temporal Anti Aliasing"
	, "Temporal Anti Aliasing."
	, "https://bkaradzic.github.io/bgfx/examples.html#tess"
);
