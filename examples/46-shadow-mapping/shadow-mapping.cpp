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

namespace CSM
{
	static float s_texelHalf = 0.0f;

	constexpr uint16_t THREAD_COUNT_PER_DIM = 8u;
	constexpr float NEAR_PLANE = 0.2f;
	constexpr float FAR_PLANE = 1000.0f;
	constexpr size_t NUM_CASCADES = 4;
	constexpr uint16_t numDepthUniforms = NUM_CASCADES / 4u + (NUM_CASCADES % 4u > 0u ? 1u : 0u);

	static glm::vec2 poissonPattern[16]{
		{ 0.0f, 0.0f },
		{  0.17109937f,  0.2446258f },
		{ -0.21000639f,  0.2215623f },
		{ -0.21870295f, -0.4121470f },
		{  0.47603912f,  0.1545703f },
		{  0.07101892f,  0.5738609f },
		{ -0.58473243f, -0.0193209f },
		{  0.20808589f, -0.5909251f },
		{ -0.50123549f,  0.4462842f },
		{ -0.35330381f,  0.7264391f },
		{ -0.32911544f, -0.8395201f },
		{ -0.58613963f, -0.7026365f },
		{  0.90719804f,  0.1760366f },
		{  0.16860312f, -0.9280076f },
		{  0.56421436f, -0.8211315f },
		{  0.99490413f, -0.1008254f },
	};

	struct DirectionalLight
	{
		glm::vec3 m_color = glm::vec3(1.0f, 1.0f, 1.0f);
		float m_intensity = 10.0f;
		glm::vec4 m_direction = glm::normalize(glm::vec4(1.0f, -3.0f, 1.0f, 0.0f));

		glm::mat4 m_cascadeTransforms[NUM_CASCADES];
		glm::vec4 m_cascadeBounds[NUM_CASCADES];

		bgfx::UniformHandle u_directionalLightParams = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_lightViewProj = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_samplingDisk = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_cascadeBounds = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_shadowMaps[NUM_CASCADES] = { BGFX_INVALID_HANDLE };
	};

	struct PBRShaderUniforms
	{
		bgfx::UniformHandle m_baseColor = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_normal = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_metallicRoughness = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_emissive = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_occlusion = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_factors = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_normalTransform = BGFX_INVALID_HANDLE;
	};

	void init(PBRShaderUniforms& uniforms)
	{
		uniforms.m_baseColor = bgfx::createUniform("s_baseColor", bgfx::UniformType::Sampler);
		uniforms.m_normal = bgfx::createUniform("s_normal", bgfx::UniformType::Sampler);
		uniforms.m_metallicRoughness = bgfx::createUniform("s_metallicRoughness", bgfx::UniformType::Sampler);
		uniforms.m_emissive = bgfx::createUniform("s_emissive", bgfx::UniformType::Sampler);
		uniforms.m_occlusion = bgfx::createUniform("s_occlusion", bgfx::UniformType::Sampler);
		uniforms.m_factors = bgfx::createUniform("u_factors", bgfx::UniformType::Vec4, 3);
		uniforms.m_normalTransform = bgfx::createUniform("u_normalTransform", bgfx::UniformType::Mat4);
	}

	void destroy(PBRShaderUniforms& uniforms)
	{
		bgfx::destroy(uniforms.m_baseColor);
		bgfx::destroy(uniforms.m_normal);
		bgfx::destroy(uniforms.m_metallicRoughness);
		bgfx::destroy(uniforms.m_emissive);
		bgfx::destroy(uniforms.m_occlusion);
		bgfx::destroy(uniforms.m_factors);
		bgfx::destroy(uniforms.m_normalTransform);
	}

	void bindUniforms(const PBRShaderUniforms& uniforms, const Dolphin::PBRMaterial& material, const glm::mat4& transform)
	{
		bgfx::setTexture(0, uniforms.m_baseColor, material.baseColorTexture);
		bgfx::setTexture(1, uniforms.m_normal, material.normalTexture);
		bgfx::setTexture(2, uniforms.m_metallicRoughness, material.metallicRoughnessTexture);
		bgfx::setTexture(3, uniforms.m_emissive, material.emissiveTexture);
		bgfx::setTexture(4, uniforms.m_occlusion, material.occlusionTexture);
		// We are going to pack our baseColorFactor, emissiveFactor, roughnessFactor
		// and metallicFactor into this uniform
		bgfx::setUniform(uniforms.m_factors, &material.baseColorFactor, 3);

		// Transforms
		bgfx::setTransform(glm::value_ptr(transform));
		glm::mat4 normalTransform{ glm::transpose(glm::inverse(transform)) };
		bgfx::setUniform(uniforms.m_normalTransform, glm::value_ptr(normalTransform));
	}

	struct SceneUniforms
	{
		float m_manualBias = 0.0f;
		float m_slopeScaleBias = 0.001f;
		float m_normalOffsetFactor = 0.01f;
		float m_texelSize = 0.0f;

		bgfx::TextureHandle m_randomTexture = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_shadowMapParams = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_cameraPos = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_randomTexture = BGFX_INVALID_HANDLE;
	};

	void init(SceneUniforms& uniforms)
	{
		uniforms.m_shadowMapParams = bgfx::createUniform("u_shadowMapParams", bgfx::UniformType::Vec4);
		uniforms.m_cameraPos = bgfx::createUniform("u_cameraPos", bgfx::UniformType::Vec4);
		uniforms.s_randomTexture = bgfx::createUniform("s_randomTexture", bgfx::UniformType::Sampler);
	}

	void destroy(SceneUniforms& uniforms)
	{
		bgfx::destroy(uniforms.m_randomTexture);
		bgfx::destroy(uniforms.m_shadowMapParams);
		bgfx::destroy(uniforms.m_cameraPos);
		bgfx::destroy(uniforms.s_randomTexture);
	}

	struct DepthReductionUniforms
	{
		bgfx::UniformHandle u_params = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_projection = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_depthSampler = BGFX_INVALID_HANDLE;
	};

	void init(DepthReductionUniforms& uniforms)
	{
		uniforms.u_params = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);
		uniforms.u_projection = bgfx::createUniform("u_projection", bgfx::UniformType::Mat4);
		uniforms.u_depthSampler = bgfx::createUniform("u_depthSampler", bgfx::UniformType::Sampler);
	}

	void destroy(DepthReductionUniforms& uniforms)
	{
		bgfx::destroy(uniforms.u_depthSampler);
		bgfx::destroy(uniforms.u_projection);
		bgfx::destroy(uniforms.u_params);
	}

	void bindUniforms(const DepthReductionUniforms& uniforms, const uint16_t width, const uint16_t height, const float projection[16])
	{
		float params[4]{ float(width), float(height), NEAR_PLANE, FAR_PLANE };
		bgfx::setUniform(uniforms.u_params, params);
		bgfx::setUniform(uniforms.u_projection, projection);
	}

	class ExampleDeferedShading : public entry::AppI
	{
	public:
		ExampleDeferedShading(const char* _name, const char* _description, const char* _url)
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

			m_caps = bgfx::getCaps();
			m_isComputeSupported = !!(m_caps->supported & BGFX_CAPS_COMPUTE);

			if (!m_isComputeSupported)
			{
				return;
			}

			
		}

		virtual int shutdown() override
		{
			

			return 0;
		}

		bool update() override
		{
			
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

		uint16_t m_shadowMapWidth = 2048u;
		float m_time;

		bgfx::ProgramHandle m_directionalShadowMapProgram;
		bgfx::ProgramHandle m_prepassProgram;
		bgfx::ProgramHandle m_pbrShader;
		bgfx::ProgramHandle m_pbrShaderWithMasking;
		bgfx::ProgramHandle m_depthReductionInitial;
		bgfx::ProgramHandle m_depthReductionGeneral;
		bgfx::ProgramHandle m_drawDepthDebugProgram;

		bgfx::TextureHandle m_shadowMaps[NUM_CASCADES];
		bgfx::FrameBufferHandle m_shadowMapFrameBuffers[NUM_CASCADES];

		bgfx::TextureHandle m_pbrFBTextures[2];
		bgfx::FrameBufferHandle m_pbrFrameBuffer = BGFX_INVALID_HANDLE;

		std::vector < bgfx::TextureHandle> m_depthReductionTargets;
		bgfx::TextureHandle m_cpuReadableDepth;

		bgfx::UniformHandle m_shadowMapDebugSampler;
		Dolphin::Model m_model;

		DirectionalLight m_directionalLight = {};

		Dolphin::ToneMapParams m_toneMapParams;
		Dolphin::ToneMapping m_toneMapPass;

		const bgfx::Caps* m_caps;
		bool m_isComputeSupported = false;
	};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	CSM::ExampleDeferedShading
	, "46-ShadowMapping"
	, "ShadowMapping."
	, "https://bkaradzic.github.io/bgfx/examples.html#tess"
);
