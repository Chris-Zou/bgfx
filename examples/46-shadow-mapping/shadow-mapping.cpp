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
	#define SAMPLER_POINT_CLAMP (BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP)

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
		bgfx::UniformHandle s_shadowMaps[NUM_CASCADES] = { BGFX_INVALID_HANDLE };
	};

	void init(DirectionalLight& light)
	{
		light.m_cascadeBounds[0].w = 0.035f;

		light.u_directionalLightParams = bgfx::createUniform("u_directionalLightParams", bgfx::UniformType::Vec4, 2);
		light.u_lightViewProj = bgfx::createUniform("u_lightViewProj", bgfx::UniformType::Mat4, NUM_CASCADES);
		light.u_samplingDisk = bgfx::createUniform("u_samplingDisk", bgfx::UniformType::Vec4, 4u);
		light.u_cascadeBounds = bgfx::createUniform("u_cascadeBounds", bgfx::UniformType::Vec4, numDepthUniforms);

		for (size_t i = 0; i < NUM_CASCADES; ++i)
		{
			char name[] = "s_shadowMap_x";;
			bx::toString(&name[12], 2, i);
			light.s_shadowMaps[i] = bgfx::createUniform(name, bgfx::UniformType::Sampler);
		}
	}

	void destroy(DirectionalLight& light)
	{
		bgfx::destroy(light.u_directionalLightParams);
		bgfx::destroy(light.u_lightViewProj);
		bgfx::destroy(light.u_samplingDisk);
		bgfx::destroy(light.u_cascadeBounds);
		for (bgfx::UniformHandle shadowMap : light.s_shadowMaps)
		{
			bgfx::destroy(shadowMap);
		}
	}

	void bindUniforms(const DirectionalLight& light, const bgfx::TextureHandle shadowMapTextures[NUM_CASCADES]) {
		bgfx::setUniform(light.u_directionalLightParams, &light, 2);
		bgfx::setUniform(light.u_lightViewProj, glm::value_ptr(light.m_cascadeTransforms[0]), NUM_CASCADES);
		bgfx::setUniform(light.u_samplingDisk, glm::value_ptr(poissonPattern[0]), 8u);
		bgfx::setUniform(light.u_cascadeBounds, light.m_cascadeBounds, NUM_CASCADES);
		for (uint8_t i = 0; i < NUM_CASCADES; i++)
		{
			bgfx::setTexture(i + 5, light.s_shadowMaps[i], shadowMapTextures[i], BGFX_SAMPLER_UVW_CLAMP);
		}
	}

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

	void bindUniforms(const SceneUniforms& uniforms, const bx::Vec3 cameraPos)
	{
		bgfx::setUniform(uniforms.m_shadowMapParams, &uniforms.m_manualBias);
		bgfx::setUniform(uniforms.m_cameraPos, &cameraPos);
		bgfx::setTexture(9, uniforms.s_randomTexture, uniforms.m_randomTexture);
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

			bgfx::setDebug(m_debug);

			bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL, 0x000000ff, 1.0f, 0);

			m_caps = bgfx::getCaps();
			m_isComputeSupported = !!(m_caps->supported & BGFX_CAPS_COMPUTE);

			if (!m_isComputeSupported)
			{
				return;
			}

			m_directionalShadowMapProgram = loadProgram("vs_directional_shadowmap", "fs_directional_shadowmap");
			m_prepassProgram = loadProgram("vs_z_prepass", "fs_z_prepass");
			m_pbrShader = loadProgram("vs_shadowed_mesh", "fs_shadowed_mesh");
			m_pbrShaderWithMasking = loadProgram("vs_shadowed_mesh", "fs_shadowed_mesh_masked");
			m_depthReductionInitial = loadProgram("cs_depth_reduction_initial", nullptr);
			m_depthReductionGeneral = loadProgram("cs_depth_reduction_general", nullptr);
			m_drawDepthDebugProgram = loadProgram("vs_texture_pass_through", "fs_texture_pass_through");

			m_model = Dolphin::loadGltfModel("meshes/Sponza/", "Sponza.gltf");

			CSM::init(m_pbrUniforms);
			CSM::init(m_sceneUniforms);
			CSM::init(m_directionalLight);
			CSM::init(m_depthReductionUniforms);

			m_shadowMapDebugSampler = bgfx::createUniform("s_input", bgfx::UniformType::Sampler);
			m_sceneUniforms.m_randomTexture = loadTexture("textures/random.png");

			m_toneMapParams.m_width = m_width;
			m_toneMapParams.m_height = m_height;
			m_toneMapParams.m_originBottomLeft = m_caps->originBottomLeft;
			m_toneMapParams.m_minLogLuminance = -5.0f;
			m_toneMapParams.m_maxLogLuminance = 10.0f;

			for (size_t i = 0; i < NUM_CASCADES; ++i)
			{
				bgfx::Attachment attachment;
				m_shadowMaps[i] = bgfx::createTexture2D(m_shadowMapWidth, m_shadowMapWidth, false, 1, bgfx::TextureFormat::D32);
				attachment.init(m_shadowMaps[i], bgfx::Access::Write);
				m_shadowMapFrameBuffers[i] = bgfx::createFrameBuffer(1, &attachment, true);
			}

			m_cpuReadableDepth = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RG16F, BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK);

			imguiCreate();

			s_texelHalf = bgfx::RendererType::Direct3D9 == m_caps->rendererType ? 0.5f : 0.0f;

			cameraCreate();
			cameraSetPosition({ 0.0f, 2.0f, 0.0f });

			m_oldWidth = 0;
			m_oldHeight = 0;
			m_oldReset = m_reset;

			m_time = 0.0f;
		}

		virtual int shutdown() override
		{
			if (!m_isComputeSupported)
			{
				return 0;
			}

			if (bgfx::isValid(m_pbrFrameBuffer))
			{
				bgfx::destroy(m_pbrFrameBuffer);
			}

			if (bgfx::isValid(m_shadowMapFrameBuffers[0]))
			{
				for (size_t i = 0; i < NUM_CASCADES; ++i) {
					bgfx::destroy(m_shadowMapFrameBuffers[i]);
				}
				for (auto texture : m_depthReductionTargets) {
					bgfx::destroy(texture);
				}
				bgfx::destroy(m_cpuReadableDepth);
			}

			m_toneMapPass.destroy();

			destroy(m_directionalLight);
			destroy(m_depthReductionUniforms);
			destroy(m_pbrUniforms);
			destroy(m_sceneUniforms);
			bgfx::destroy(m_shadowMapDebugSampler);
			Dolphin::destroy(m_model);
			bgfx::destroy(m_drawDepthDebugProgram);
			bgfx::destroy(m_depthReductionGeneral);
			bgfx::destroy(m_depthReductionInitial);
			bgfx::destroy(m_directionalShadowMapProgram);
			bgfx::destroy(m_prepassProgram);
			bgfx::destroy(m_pbrShader);
			bgfx::destroy(m_pbrShaderWithMasking);

			cameraDestroy();

			imguiDestroy();

			bgfx::shutdown();

			return 0;
		}

		void renderMeshes(const Dolphin::MeshGroup& meshes, const bx::Vec3& cameraPos, const uint64_t state, const bgfx::ProgramHandle program, const bgfx::ViewId viewId) const
		{
			for (size_t i = 0; i < meshes.meshes.size(); ++i)
			{
				const auto&  mesh = meshes.meshes[i];
				const auto& transform = meshes.transforms[i];
				const auto& material = meshes.materials[i];

				bgfx::setState(state);
				bindUniforms(m_pbrUniforms, material, transform);
				bindUniforms(m_sceneUniforms, cameraPos);
				bindUniforms(m_directionalLight, m_shadowMaps);

				mesh.setBuffers();

				bgfx::submit(viewId, program);
			}
		}

		uint16_t getDispatchSize(uint16_t dim, uint16_t threadCount) const
		{
			uint16_t dispatchSize = dim / threadCount;
			dispatchSize += dim % threadCount > 0 ? 1 : 0;
			return dispatchSize;
		}

		void setupDepthReductionTargets(uint16_t width, uint16_t height)
		{
			for (bgfx::TextureHandle texture : m_depthReductionTargets)
			{
				bgfx::destroy(texture);
			}

			m_depthReductionTargets.clear();

			while (width > 1 || height > 1)
			{
				width = getDispatchSize(width, THREAD_COUNT_PER_DIM);
				height = getDispatchSize(height, THREAD_COUNT_PER_DIM);
				bgfx::TextureHandle texture = bgfx::createTexture2D(width, height, false, 0, bgfx::TextureFormat::RG16F, BGFX_TEXTURE_COMPUTE_WRITE);
				m_depthReductionTargets.push_back(texture);
			}
		}

		bool update() override
		{
			if (entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState))
			{
				return false;
			}

			if (!m_isComputeSupported)
				return false;

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

				m_toneMapParams.m_width = m_width;
				m_toneMapParams.m_height = m_height;

				m_pbrFBTextures[0] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::RGBA16F, (uint64_t(msaa + 1) << BGFX_TEXTURE_RT_MSAA_SHIFT));

				const uint64_t textureFlags = BGFX_TEXTURE_RT | (uint64_t(msaa + 1) << BGFX_TEXTURE_RT_MSAA_SHIFT) | BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP;

				bgfx::TextureFormat::Enum depthFormat = bgfx::TextureFormat::D32;
				m_pbrFBTextures[1] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, depthFormat, textureFlags);

				bgfx::setName(m_pbrFBTextures[0], "HDR Buffer");
				bgfx::setName(m_pbrFBTextures[1], "Depth Buffer");

				m_pbrFrameBuffer = bgfx::createFrameBuffer(BX_COUNTOF(m_pbrFBTextures), m_pbrFBTextures, true);

				setupDepthReductionTargets(uint16_t(m_width), uint16_t(m_height));
			}

			imguiBeginFrame(m_mouseState.m_mx, m_mouseState.m_my, (m_mouseState.m_buttons[entry::MouseButton::Left] ? IMGUI_MBUT_LEFT : 0) | (m_mouseState.m_buttons[entry::MouseButton::Right] ? IMGUI_MBUT_RIGHT : 0) | (m_mouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0), m_mouseState.m_mz, uint16_t(m_width), uint16_t(m_height));

			showExampleDialog(this);

			ImGui::SetNextWindowPos(
				ImVec2(m_width - m_width / 5.0f - 10.0f, 10.0f), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(
				ImVec2(m_width / 5.0f, m_height / 3.0f), ImGuiCond_FirstUseEver);
			ImGui::Begin("Settings", NULL, 0);

			ImGui::DragFloat("Total Brightness", &m_directionalLight.m_intensity, 0.5f, 0.0f, 250.0f);
			ImGui::Checkbox("Update Lights", &m_updateLights);

			if (!m_updateLights) {
				ImGui::SliderFloat3("Light Direction", glm::value_ptr(m_directionalLight.m_direction), -1.0f, 1.0f);
				m_directionalLight.m_direction = glm::normalize(m_directionalLight.m_direction);
			}

			ImGui::SliderFloat("Manual Bias", &m_sceneUniforms.m_manualBias, 0.0f, 0.01f);
			ImGui::Text("Slope Scale Bias Factor");
			ImGui::SliderFloat("Slope", &m_sceneUniforms.m_slopeScaleBias, 0.0f, 0.01f);
			ImGui::Text("Normal Offset Bias");
			ImGui::SliderFloat("Normal", &m_sceneUniforms.m_normalOffsetFactor, 0.0f, 0.05f);
			ImGui::Text("Poisson Disk Size");
			ImGui::SliderFloat("Disk", &m_directionalLight.m_cascadeBounds[0].w, 0.001f, 0.1f);

			ImGui::End();

			imguiEndFrame();

			bgfx::touch(0);

			bgfx::ViewId viewCount = 0;
			bgfx::ViewId zPrepass = viewCount++;
			bgfx::setViewFrameBuffer(zPrepass, m_pbrFrameBuffer);
			bgfx::setViewName(zPrepass, "Z Prepass");
			bgfx::setViewRect(zPrepass, 0, 0, uint16_t(m_width), uint16_t(m_height));
			bgfx::setViewClear(zPrepass, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);

			bgfx::ViewId depthReductionPass = viewCount++;
			bgfx::setViewName(depthReductionPass, "Depth Reduction");

			bgfx::ViewId shadowPass[NUM_CASCADES];
			for (size_t i = 0; i < NUM_CASCADES; ++i)
			{
				shadowPass[i] = viewCount++;
				bgfx::setViewFrameBuffer(shadowPass[i], m_shadowMapFrameBuffers[i]);
				bgfx::setViewName(shadowPass[i], "Shadow Map");
				bgfx::setViewRect(shadowPass[i], 0, 0, m_shadowMapWidth, m_shadowMapWidth);
				bgfx::setViewClear(shadowPass[i], BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);
			}

			bgfx::ViewId meshPass = viewCount++;
			bgfx::setViewFrameBuffer(meshPass, m_pbrFrameBuffer);
			bgfx::setViewName(meshPass, "Draw Meshes");
			bgfx::setViewRect(meshPass, 0, 0, uint16_t(m_width), uint16_t(m_height));

			int64_t now = bx::getHPCounter();
			static int64_t last = now;
			const int64_t frameTime = now - last;
			last = now;
			const double freq = double(bx::getHPFrequency());
			const float deltaTime = (float)(frameTime / freq);
			m_time += deltaTime;

			float fov = 60.0f;
			float proj[16];
			bx::mtxProj(proj, fov, float(m_width) / float(m_height), NEAR_PLANE, FAR_PLANE, m_caps->homogeneousDepth);

			if (m_updateLights)
			{
				m_directionalLight.m_direction = glm::normalize(glm::vec4{ bx::cos(0.2f * m_time), -bx::abs(bx::sin(0.2f * m_time)), bx::cos(0.2f * m_time) * 0.2f, 0.0f });
			}

			float view[16];
			cameraUpdate(0.1f * deltaTime, m_mouseState);
			cameraGetViewMtx(view);

			bgfx::setViewTransform(zPrepass, view, proj);
			bgfx::setViewTransform(meshPass, view, proj);

			bx::Vec3 cameraPos = cameraGetPosition();

			{
				uint64_t statePrepass = 0
					| BGFX_STATE_WRITE_Z
					| BGFX_STATE_DEPTH_TEST_LESS
					| BGFX_STATE_CULL_CCW
					| BGFX_STATE_MSAA;

				renderMeshes(m_model.opaqueMeshes, cameraPos, statePrepass, m_prepassProgram, zPrepass);
			}

			{
				uint16_t dispatchX = getDispatchSize(uint16_t(m_width), THREAD_COUNT_PER_DIM);
				uint16_t dispatchY = getDispatchSize(uint16_t(m_height), THREAD_COUNT_PER_DIM);

				bindUniforms(m_depthReductionUniforms, uint16_t(m_width), uint16_t(m_height), proj);
				bgfx::setTexture(0, m_depthReductionUniforms.u_depthSampler, m_pbrFBTextures[1], SAMPLER_POINT_CLAMP);
				bgfx::setImage(1, m_depthReductionTargets[0], 0, bgfx::Access::Write, bgfx::TextureFormat::RG16F);
				bgfx::dispatch(depthReductionPass, m_depthReductionInitial, dispatchX, dispatchY, 1);

				for (size_t i = 1; i < m_depthReductionTargets.size(); ++i)
				{
					bindUniforms(m_depthReductionUniforms, dispatchX, dispatchY, proj);
					dispatchX = getDispatchSize(dispatchX, THREAD_COUNT_PER_DIM);
					dispatchY = getDispatchSize(dispatchY, THREAD_COUNT_PER_DIM);

					bgfx::setImage(0, m_depthReductionTargets[i - 1], 0, bgfx::Access::Read, bgfx::TextureFormat::RG16F);
					bgfx::setImage(1, m_depthReductionTargets[i], 0, bgfx::Access::Write, bgfx::TextureFormat::RG16F);
					bgfx::dispatch(depthReductionPass, m_depthReductionGeneral, dispatchX, dispatchY, 1);
				}
			}

			{
				bgfx::blit(shadowPass[0], m_cpuReadableDepth, 0, 0, m_depthReductionTargets[m_depthReductionTargets.size() - 1], 0, 0);
				bgfx::readTexture(m_cpuReadableDepth, m_depthData, 0);
				float minDepth = bx::halfToFloat(m_depthData[0]);
				float maxDepth = bx::halfToFloat(m_depthData[1]);

				glm::mat4 viewProj = glm::make_mat4(proj) * glm::make_mat4(view);
				glm::mat4 invViewProj = glm::inverse(viewProj);

				float minWorldDepth = minDepth * (FAR_PLANE - NEAR_PLANE) + NEAR_PLANE;
				float maxWorldDepth = maxDepth * (FAR_PLANE - NEAR_PLANE) + NEAR_PLANE;

				float depthRatio = bx::pow(maxWorldDepth / minWorldDepth, 1.0f / NUM_CASCADES);
				glm::vec2 cascadeMinMax[NUM_CASCADES] = {};
				cascadeMinMax[0] = { minWorldDepth, minWorldDepth * depthRatio };
				for (int cascadeIdx = 1; cascadeIdx < NUM_CASCADES; ++cascadeIdx)
				{
					cascadeMinMax[cascadeIdx] = {cascadeMinMax[cascadeIdx -1].y, cascadeMinMax[cascadeIdx-1].y * depthRatio};
				}

				for (int casIdx = 0; casIdx < NUM_CASCADES; ++casIdx)
				{
					m_directionalLight.m_cascadeBounds[casIdx].z = (proj[10] * cascadeMinMax[casIdx].y + proj[14]) / (proj[11] * cascadeMinMax[casIdx].y);
				}

				float clipNear = m_caps->homogeneousDepth ? -1.0f : 0.0f;
				glm::vec4 clipFrustum[] =
				{
					glm::vec4(-1.0f,  1.0f, clipNear, 1.0f), // top left near
					glm::vec4(1.0f,  1.0f, clipNear, 1.0f), // top right near
					glm::vec4(1.0f, -1.0f, clipNear, 1.0f), // bottom right near
					glm::vec4(-1.0f, -1.0f, clipNear, 1.0f), // bottom left near
					glm::vec4(-1.0f,  1.0f, 1.0f, 1.0f), // top left far
					glm::vec4(1.0f,  1.0f, 1.0f, 1.0f), // top right far
					glm::vec4(1.0f, -1.0f, 1.0f, 1.0f), // bottom right far
					glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f), // botom left far
				};

				for (int casIdx = 0; casIdx < NUM_CASCADES; ++casIdx)
				{
					float cascMin = (cascadeMinMax[casIdx].x - NEAR_PLANE) / (FAR_PLANE - NEAR_PLANE);
					float cascMax = (cascadeMinMax[casIdx].y - NEAR_PLANE) / (FAR_PLANE - NEAR_PLANE);

					glm::vec4 frustumCorners[8];
					for (size_t i = 0; i < 8; ++i)
					{
						frustumCorners[i] = invViewProj * clipFrustum[i];
						frustumCorners[i] /= frustumCorners[i].w;
					}

					glm::vec4 center{ 0.0f };
					for (size_t i = 0; i < 4; ++i)
					{
						glm::vec4 cornerRay = frustumCorners[i + 4] - frustumCorners[i];
						frustumCorners[i] = cascMin * cornerRay + frustumCorners[i];
						frustumCorners[i + 4] = cascMax * cornerRay + frustumCorners[i];
						center += frustumCorners[i] + frustumCorners[i + 4];
					}
					center /= 8.0f;

					glm::vec3 up = bx::abs(m_directionalLight.m_direction.y) != 1.0f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
					glm::mat4 lightView = glm::lookAt(glm::vec3(center - m_directionalLight.m_direction), glm::vec3(center), up);

					for (size_t i = 0; i < BX_COUNTOF(frustumCorners); ++i)
					{
						frustumCorners[i] = lightView * frustumCorners[i];
					}

					glm::vec4 min = frustumCorners[0];
					glm::vec4 max = frustumCorners[0];

					for (size_t i = 0; i < BX_COUNTOF(frustumCorners); ++i)
					{
						min = glm::min(min, frustumCorners[i]);
						max = glm::max(max, frustumCorners[i]);
					}

					glm::vec4 bbMin = glm::vec4(m_model.boundingBox.m_min, 1.0f);
					glm::vec4 bbMax = glm::vec4(m_model.boundingBox.m_max, 1.0f);
					glm::vec4 bbCorners[8] = {
						{bbMin.x, bbMax.y, bbMax.z, 1.0f },
						{bbMax.x, bbMax.y, bbMax.z, 1.0f },
						{bbMax.x, bbMin.y, bbMax.z, 1.0f },
						{bbMin.x, bbMin.y, bbMax.z, 1.0f },
						{bbMin.x, bbMax.y, bbMin.z, 1.0f },
						{bbMax.x, bbMax.y, bbMin.z, 1.0f },
						{bbMax.x, bbMin.y, bbMin.z, 1.0f },
						{bbMin.x, bbMin.y, bbMin.z, 1.0f },
					};

					bbMin = lightView * bbCorners[0];
					bbMax = lightView * bbCorners[0];
					for (size_t i = 0; i < BX_COUNTOF(bbCorners); ++i)
					{
						glm::vec4 bbCornerLightSpace = lightView * bbCorners[i];
						bbMin = glm::min(bbCornerLightSpace, bbMin);
						bbMax = glm::max(bbCornerLightSpace, bbMax);
					}

					min.x = bx::max(bbMin.x, min.x);
					max.x = bx::max(bbMax.x, max.x);
					min.y = bx::max(bbMin.y, min.y);
					max.y = bx::min(bbMax.y, max.y);
					min.z = bx::min(bbMin.z, min.z);
					max.z = bx::max(bbMax.z, max.z);

					uint64_t stateShadowMapping = 0
						| BGFX_STATE_WRITE_Z
						| BGFX_STATE_CULL_CW
						| BGFX_STATE_DEPTH_TEST_LESS;

					float orthoProjectionRaw[16];
					bx::mtxOrtho(orthoProjectionRaw, min.x, max.x, min.y, max.y, max.z, min.z, 0.0f, m_caps->homogeneousDepth);
					glm::mat4 orthoProjection = glm::make_mat4(orthoProjectionRaw);

					m_directionalLight.m_cascadeBounds[casIdx].x = max.x - min.x;
					m_directionalLight.m_cascadeBounds[casIdx].y = max.y - min.y;

					lightView = glm::lookAt(glm::vec3(center - m_directionalLight.m_direction), glm::vec3(center), up);

					bgfx::setViewTransform(shadowPass[casIdx], glm::value_ptr(lightView), glm::value_ptr(orthoProjection));
					m_directionalLight.m_cascadeTransforms[casIdx] = orthoProjection * lightView;

					for (size_t i = 0; i < m_model.opaqueMeshes.meshes.size(); ++i)
					{
						const auto& mesh = m_model.opaqueMeshes.meshes[i];
						const auto& transform = m_model.opaqueMeshes.transforms[i];

						bgfx::setState(stateShadowMapping);
						bgfx::setTransform(glm::value_ptr(transform));
						mesh.setBuffers();
						bgfx::submit(shadowPass[casIdx], m_directionalShadowMapProgram);
					}
				}
			}

			uint64_t stateOpaque = 0
				| BGFX_STATE_WRITE_RGB
				| BGFX_STATE_WRITE_A
				| BGFX_STATE_CULL_CCW
				| BGFX_STATE_MSAA
				| BGFX_STATE_DEPTH_TEST_LEQUAL;

			uint64_t stateTransparent = 0
				| BGFX_STATE_WRITE_RGB
				| BGFX_STATE_WRITE_A
				| BGFX_STATE_DEPTH_TEST_LESS
				| BGFX_STATE_CULL_CCW
				| BGFX_STATE_MSAA
				| BGFX_STATE_BLEND_ALPHA;

			renderMeshes(m_model.opaqueMeshes, cameraPos, stateOpaque, m_pbrShader, meshPass);

			renderMeshes(m_model.maskedMeshes, cameraPos, stateOpaque, m_pbrShaderWithMasking, meshPass);

			renderMeshes(m_model.transparentMeshes, cameraPos, stateTransparent, m_pbrShader, meshPass);

			viewCount = m_toneMapPass.render(m_pbrFBTextures[0], m_toneMapParams, deltaTime, viewCount);

			bgfx::ViewId debugShadowPass = viewCount++;
			bgfx::setViewRect(debugShadowPass, 0, uint16_t(m_height) - 256u, 256, 256);

			float debugProjection[16];
			bx::mtxOrtho(debugProjection, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, m_caps->homogeneousDepth);
			bgfx::setViewTransform(debugShadowPass, nullptr, debugProjection);
			bgfx::setTexture(0, m_shadowMapDebugSampler, m_shadowMaps[0], BGFX_SAMPLER_UVW_CLAMP);
			bgfx::setState(BGFX_STATE_WRITE_RGB);
			Dolphin::ToneMapping::setScreenSpaceQuad(m_shadowMapWidth, m_shadowMapWidth, m_caps->originBottomLeft);
			bgfx::submit(debugShadowPass, m_drawDepthDebugProgram);

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

		PBRShaderUniforms m_pbrUniforms = {};
		SceneUniforms m_sceneUniforms = {};
		DepthReductionUniforms m_depthReductionUniforms = {};

		bgfx::UniformHandle m_shadowMapDebugSampler;
		Dolphin::Model m_model;

		DirectionalLight m_directionalLight = {};

		Dolphin::ToneMapParams m_toneMapParams;
		Dolphin::ToneMapping m_toneMapPass;

		const bgfx::Caps* m_caps;
		bool m_isComputeSupported = true;
		bool m_updateLights = true;

		uint16_t m_depthData[2] = { 0, bx::kHalfFloatOne };
	};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	CSM::ExampleDeferedShading
	, "46-ShadowMapping"
	, "ShadowMapping."
	, "https://bkaradzic.github.io/bgfx/examples.html#tess"
);
