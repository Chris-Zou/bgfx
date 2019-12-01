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

#include <array>
#include <glm/matrix.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "ShaderCompiler/ShaderCompiler.h"
#include "Renderer/toneMappingRender.h"
#include "SceneManager/sceneMangement.h"
#include "SceneManager/gltf_model_loading.h"

namespace ibl
{
	static float s_texelHalf = 0.0f;

	class BrdfLutCreator
	{
	public:
		void init()
		{
			const std::string brdfLutShaderName = "../43-pbr-ibl/cs_brdf_lut.sc";
			m_brdfProgram = Dolphin::compileComputeShader(brdfLutShaderName.c_str());

			uint64_t lutFlags = BGFX_TEXTURE_COMPUTE_WRITE | BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP;
			m_brdfLut = bgfx::createTexture2D(m_width, m_width, false, 1, bgfx::TextureFormat::RG16F, lutFlags);
			bgfx::setName(m_brdfLut, "Smith BRDF LUT");
		}

		bgfx::TextureHandle getLut()
		{
			return m_brdfLut;
		}

		void renderLUT(bgfx::ViewId view)
		{
			const uint16_t threadCount = 16u;
			bgfx::setViewName(view, "BRDF LUT creation pass");

			bgfx::setImage(0, m_brdfLut, 0, bgfx::Access::Write, bgfx::TextureFormat::RG16F);
			bgfx::dispatch(view, m_brdfProgram, m_width / threadCount, m_width / threadCount, 1);

			m_rendered = true;
		}

		void destroy()
		{
			bgfx::destroy(m_brdfProgram);
			if (m_destroyTextures)
			{
				bgfx::destroy(m_brdfLut);
			}
		}

	public:
		uint16_t m_width = 128u;
		bgfx::TextureHandle m_brdfLut;
		bgfx::ProgramHandle m_brdfProgram = BGFX_INVALID_HANDLE;

		bool m_rendered = false;
		bool m_destroyTextures = true;
	};

	class CubeMapFilterer
	{
	public:
		void init()
		{
			m_prefilteringProgram = Dolphin::compileComputeShader("../43-pbr-ibl/cs_prefilter_env_map.sc");
			m_irradianceProgram = Dolphin::compileComputeShader("../43-pbr-ibl/cs_irradiance.sc");

			uint64_t flags = BGFX_TEXTURE_COMPUTE_WRITE;
			u_sourceCubeMap = bgfx::createUniform("u_source", bgfx::UniformType::Sampler);
			u_params = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);
			m_filteredCubeMap = bgfx::createTextureCube(m_width, true, 1, bgfx::TextureFormat::RGBA16F, flags);
			m_irradianceMap = bgfx::createTextureCube(m_irradianceMapSize, false, 1, bgfx::TextureFormat::RGBA16F, flags);
			bgfx::setName(m_filteredCubeMap, "Prefilter Env Map");
		}

		bgfx::TextureHandle getPrefilteredMap()
		{
			return m_filteredCubeMap;
		}

		bgfx::TextureHandle getIrradianceMap()
		{
			return m_irradianceMap;
		}

		void render(bgfx::ViewId view)
		{
			const uint16_t threadCount = 8u;
			bgfx::setViewName(view, "Env Map Filtering Pass");

			float maxMipLevel = bx::log2(float(m_width));
			for (float mipLevel = 0; mipLevel <= maxMipLevel; ++mipLevel)
			{
				uint16_t mipWidth = m_width / uint16_t(bx::pow(2.0f, mipLevel));
				float roughness = mipLevel / maxMipLevel;
				float params[] = { roughness, float(mipLevel), float(m_width), 0.0f };

				bgfx::setUniform(u_params, params);
				bgfx::setTexture(0, u_sourceCubeMap, m_sourceCubeMap);
				bgfx::setImage(1, m_filteredCubeMap, uint8_t(mipLevel), bgfx::Access::Write, bgfx::TextureFormat::RGBA16F);
				bgfx::dispatch(view, m_prefilteringProgram, mipWidth / threadCount, mipWidth / threadCount, 1);
			}
		}

		void destroy()
		{
			bgfx::destroy(m_prefilteringProgram);
			bgfx::destroy(m_irradianceProgram);
			bgfx::destroy(u_sourceCubeMap);
			bgfx::destroy(u_params);
			if (m_isDestroyTextures) {
				bgfx::destroy(m_sourceCubeMap);
				bgfx::destroy(m_filteredCubeMap);
				bgfx::destroy(m_irradianceMap);
			}
		}

	public:
		uint16_t m_width = 0u;
		uint16_t m_irradianceMapSize = 64u;

		bgfx::UniformHandle u_params = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_sourceCubeMap = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle m_sourceCubeMap = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle m_filteredCubeMap = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle m_irradianceMap = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle m_prefilteringProgram = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle m_irradianceProgram = BGFX_INVALID_HANDLE;

		bool m_isRendered = false;
		bool m_isDestroyTextures = true;
	};

	struct SkyboxUniforms
	{
		bgfx::UniformHandle s_envMap = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_invRotationViewProj = BGFX_INVALID_HANDLE;
	};

	void init(SkyboxUniforms& uniforms)
	{
		uniforms.s_envMap = bgfx::createUniform("s_envMap", bgfx::UniformType::Sampler);
		uniforms.u_invRotationViewProj = bgfx::createUniform("u_invRotationViewProj", bgfx::UniformType::Mat4);
	}

	void destroy(SkyboxUniforms& uniforms) {
		bgfx::destroy(uniforms.s_envMap);
		bgfx::destroy(uniforms.u_invRotationViewProj);
	}

	struct SceneUniforms
	{
		bgfx::UniformHandle u_cameraPos = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_envParams = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_brdfLUT = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_prefilteredEnv = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_irradiance = BGFX_INVALID_HANDLE;

	};

	void init(SceneUniforms& uniforms)
	{
		uniforms.u_cameraPos = bgfx::createUniform("u_cameraPos", bgfx::UniformType::Vec4);
		uniforms.u_envParams = bgfx::createUniform("u_envParams", bgfx::UniformType::Vec4);
		uniforms.s_brdfLUT = bgfx::createUniform("s_brdfLUT", bgfx::UniformType::Sampler);
		uniforms.s_prefilteredEnv = bgfx::createUniform("s_prefilteredEnv", bgfx::UniformType::Sampler);
		uniforms.s_irradiance = bgfx::createUniform("s_irradiance", bgfx::UniformType::Sampler);

	}

	void destroy(SceneUniforms& uniforms)
	{
		bgfx::destroy(uniforms.u_cameraPos);
		bgfx::destroy(uniforms.u_envParams);
		bgfx::destroy(uniforms.s_brdfLUT);
		bgfx::destroy(uniforms.s_prefilteredEnv);
		bgfx::destroy(uniforms.s_irradiance);
	}

	struct PBRShaderUniforms
	{
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
		// We are going to pack our baseColorFactor, emissiveFactor, roughnessFactor
		// and metallicFactor into this uniform
		bgfx::setUniform(uniforms.u_factors, &material.baseColorFactor, 3);

		// Transforms
		bgfx::setTransform(glm::value_ptr(transform));
		glm::mat4 normalTransform{ glm::transpose(glm::inverse(transform)) };
		bgfx::setUniform(uniforms.u_normalTransform, glm::value_ptr(normalTransform));
	}

	class ExamplePBR_IBL : public entry::AppI
	{
	public:
		ExamplePBR_IBL(const char* _name, const char* _description, const char* _url)
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
			m_computeSupported = !!(m_caps->supported & BGFX_CAPS_COMPUTE);

			if (!m_computeSupported) {
				return;
			}

			m_skyboxProgram = Dolphin::compileGraphicsShader("../43-pbr-ibl/vs_skybox.sc", "../43-pbr-ibl/fs_skybox.sc", "../43-pbr-ibl/varying.def.sc");
			m_pbrIblProgram = Dolphin::compileGraphicsShader("../43-pbr-ibl/vs_pbr_ibl.sc", "../43-pbr-ibl/fs_pbr_ibl.sc", "../43-pbr-ibl/varying.def.sc");
			m_pbrIblProgramWithMasking = Dolphin::compileGraphicsShader("../43-pbr-ibl/vs_pbr_ibl.sc", "../43-pbr-ibl/fs_pbr_ibl_with_masking.sc", "../43-pbr-ibl/varying.def.sc");

			ibl::init(m_pbrUniforms);
			ibl::init(m_sceneUniforms);
			ibl::init(m_skyboxUniforms);

			m_model = Dolphin::loadGltfModel("meshes/FlightHelmet/", "FlightHelmet.gltf");

			m_toneMapParams.m_width = m_width;
			m_toneMapParams.m_width = m_height;
			m_toneMapParams.m_originBottomLeft = m_caps->originBottomLeft;
			m_toneMapPass.init(m_caps);

			m_brdfLutCreator.init();

			m_envMap = loadTexture("textures/pisa_with_mips.ktx");
			m_prefilteredEnvMapCreator.m_sourceCubeMap = m_envMap;
			m_prefilteredEnvMapCreator.m_width = 1024u; // Based off size of pisa_with_mips.ktx
			m_prefilteredEnvMapCreator.init();

			// Imgui.
			imguiCreate();

			s_texelHalf = bgfx::RendererType::Direct3D9 == m_caps->rendererType ? 0.5f : 0.0f;

			// Init camera
			cameraCreate();
			cameraSetPosition({ -3.5f, 0.0f, 7.0f });
			cameraSetHorizontalAngle(bx::atan2(3.5f, -7.0f));
			cameraSetVerticalAngle(bx::toRad(-10.0f));

			m_oldWidth = 0;
			m_oldHeight = 0;
			m_oldReset = m_reset;

			m_time = 0.0f;
		}

		virtual int shutdown() override
		{
			if (m_computeSupported)
			{
				if (bgfx::isValid(m_hdrFrameBuffer))
				{
					bgfx::destroy(m_hdrFrameBuffer);
				}
				m_toneMapPass.destroy();
				m_prefilteredEnvMapCreator.destroy();
				m_brdfLutCreator.destroy();

				destroy(m_model);

				destroy(m_pbrUniforms);
				destroy(m_sceneUniforms);
				destroy(m_skyboxUniforms);

				bgfx::destroy(m_skyboxProgram);
				bgfx::destroy(m_pbrIblProgram);
				bgfx::destroy(m_pbrIblProgramWithMasking);

				cameraDestroy();
				imguiDestroy();
			}

			// Shutdown bgfx.
			bgfx::shutdown();

			return 0;
		}

		void initializeFrameBuffers() {
			// Recreate variable size render targets when resolution changes.
			m_oldWidth = m_width;
			m_oldHeight = m_height;
			m_oldReset = m_reset;

			uint32_t msaa = (m_reset & BGFX_RESET_MSAA_MASK) >> BGFX_RESET_MSAA_SHIFT;

			if (bgfx::isValid(m_hdrFrameBuffer))
			{
				bgfx::destroy(m_hdrFrameBuffer);
			}

			m_toneMapParams.m_width = m_width;
			m_toneMapParams.m_height = m_height;

			const uint64_t tsFlags = 0
				| BGFX_SAMPLER_MIN_POINT
				| BGFX_SAMPLER_MAG_POINT
				| BGFX_SAMPLER_MIP_POINT
				| BGFX_SAMPLER_U_CLAMP
				| BGFX_SAMPLER_V_CLAMP;

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
		}

		void renderMeshes(
			const Dolphin::MeshGroup& meshes,
			const uint64_t state,
			const bgfx::ProgramHandle program,
			const bgfx::ViewId viewId
		)
		{
			// Render all our opaque meshes
			for (size_t i = 0; i < meshes.meshes.size(); ++i) {
				const auto& mesh = meshes.meshes[i];
				const auto& transform = meshes.transforms[i];
				const auto& material = meshes.materials[i];

				bgfx::setState(state);
				bindUniforms(m_pbrUniforms, material, transform);
				bgfx::setTexture(5, m_sceneUniforms.s_brdfLUT, m_brdfLutCreator.getLut());
				bgfx::setTexture(6, m_sceneUniforms.s_prefilteredEnv, m_prefilteredEnvMapCreator.getPrefilteredMap());
				bgfx::setTexture(7, m_sceneUniforms.s_irradiance, m_prefilteredEnvMapCreator.getIrradianceMap());

				mesh.setBuffers();

				bgfx::submit(viewId, program);
			}
		}

		bool update() override
		{
			if (entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState)) {
				return false;
			}

			if (!m_computeSupported) {
				return false;
			}

			bgfx::ViewId viewId = 0;

			if (!m_brdfLutCreator.m_rendered) {
				m_brdfLutCreator.renderLUT(viewId);
			}
			// Have to still skip using this viewId, or reset the views to remove all the associated state.
			viewId++;

			if (!m_prefilteredEnvMapCreator.m_isRendered) {
				m_prefilteredEnvMapCreator.render(viewId);
			}
			viewId++;

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

			ImGui::RadioButton("Single Scattering", &m_iblMode, 0);
			ImGui::RadioButton("Multi-scattering, standard Fresnel", &m_iblMode, 1);
			ImGui::RadioButton("Multiscattering, roughness depedent", &m_iblMode, 2);

			ImGui::End();

			imguiEndFrame();


			// Set views.
			bgfx::ViewId skyboxPass = viewId++;
			bgfx::setViewName(skyboxPass, "Skybox");
			bgfx::setViewClear(skyboxPass, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
			bgfx::setViewRect(skyboxPass, 0, 0, bgfx::BackbufferRatio::Equal);
			bgfx::setViewFrameBuffer(skyboxPass, m_hdrFrameBuffer);

			bgfx::ViewId meshPass = viewId++;
			bgfx::setViewRect(meshPass, 0, 0, uint16_t(m_width), uint16_t(m_height));
			bgfx::setViewFrameBuffer(meshPass, m_hdrFrameBuffer);
			bgfx::setViewName(meshPass, "Draw Meshes");

			int64_t now = bx::getHPCounter();
			static int64_t last = now;
			const int64_t frameTime = now - last;
			last = now;
			const double freq = double(bx::getHPFrequency());
			const float deltaTime = bx::max(0.0001f, (float)(frameTime / freq));
			m_time += deltaTime;

			float orthoProjection[16];
			bx::mtxOrtho(orthoProjection, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f, 0.0f, m_caps->homogeneousDepth);

			float proj[16];
			bx::mtxProj(proj, 60.0f, float(m_width) / float(m_height), 0.1f, 1000.0f, bgfx::getCaps()->homogeneousDepth);

			// Update camera
			float view[16];
			cameraUpdate(0.5f * deltaTime, m_mouseState);
			cameraGetViewMtx(view);
			bx::Vec3 cameraPos = cameraGetPosition();

			float viewCopy[16] = {};
			bx::memCopy(viewCopy, view, 16 * sizeof(view[0]));
			// Remove translation...
			viewCopy[12] = 0.0f;
			viewCopy[13] = 0.0f;
			viewCopy[14] = 0.0f;

			float rotationViewProj[16] = {};
			bx::mtxMul(rotationViewProj, viewCopy, proj);
			float invRotationViewProj[16] = {};
			bx::mtxInverse(invRotationViewProj, rotationViewProj);

			// Render skybox into view hdrSkybox.
			bgfx::setTexture(0, m_skyboxUniforms.s_envMap, m_prefilteredEnvMapCreator.getPrefilteredMap());
			bgfx::setUniform(m_skyboxUniforms.u_invRotationViewProj, invRotationViewProj);
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
			bgfx::setViewTransform(skyboxPass, nullptr, orthoProjection);
			Dolphin::ToneMapping::setScreenSpaceQuad((float)m_width, (float)m_height, true);
			bgfx::submit(skyboxPass, m_skyboxProgram);

			// Set view and projection matrix
			uint64_t stateOpaque = 0
				| BGFX_STATE_WRITE_RGB
				| BGFX_STATE_WRITE_A
				| BGFX_STATE_CULL_CCW
				| BGFX_STATE_MSAA
				| BGFX_STATE_WRITE_Z
				| BGFX_STATE_DEPTH_TEST_LESS;

			uint64_t stateTransparent = 0
				| BGFX_STATE_WRITE_RGB
				| BGFX_STATE_WRITE_A
				| BGFX_STATE_DEPTH_TEST_LESS
				| BGFX_STATE_CULL_CCW
				| BGFX_STATE_MSAA
				| BGFX_STATE_BLEND_ALPHA;

			bgfx::setViewTransform(meshPass, view, proj);

			float envParams[] = { bx::log2(float(m_prefilteredEnvMapCreator.m_width)), float(m_iblMode), 0.0f, 0.0f };
			bgfx::setUniform(m_sceneUniforms.u_envParams, envParams);
			bgfx::setUniform(m_sceneUniforms.u_cameraPos, &cameraPos.x);

			renderMeshes(m_model.opaqueMeshes, stateOpaque, m_pbrIblProgram, meshPass);
			renderMeshes(m_model.maskedMeshes, stateOpaque, m_pbrIblProgramWithMasking, meshPass);
			renderMeshes(m_model.transparentMeshes, stateTransparent, m_pbrIblProgram, meshPass);

			m_toneMapPass.render(m_hdrFbTextures[0], m_toneMapParams, deltaTime, viewId);

			bgfx::frame();

			return true;
		}

	public :
		entry::MouseState m_mouseState;

		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_debug;
		uint32_t m_reset;

		uint32_t m_oldWidth;
		uint32_t m_oldHeight;
		uint32_t m_oldReset;

		bgfx::ProgramHandle m_skyboxProgram = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle m_pbrIblProgram = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle m_pbrIblProgramWithMasking = BGFX_INVALID_HANDLE;

		// PBR IRB Textures and LUT
		bgfx::TextureHandle m_envMap = BGFX_INVALID_HANDLE;
		// Buffer to put final outputs into
		bgfx::TextureHandle m_hdrFbTextures[2];
		bgfx::FrameBufferHandle m_hdrFrameBuffer = BGFX_INVALID_HANDLE;

		Dolphin::ToneMapParams m_toneMapParams;
		Dolphin::ToneMapping m_toneMapPass;

		BrdfLutCreator m_brdfLutCreator;
		CubeMapFilterer m_prefilteredEnvMapCreator;

		Dolphin::Model m_model;
		PBRShaderUniforms m_pbrUniforms;
		SceneUniforms m_sceneUniforms;
		SkyboxUniforms m_skyboxUniforms;

		const bgfx::Caps* m_caps;
		float m_time;

		bool m_computeSupported = true;
		int m_iblMode = 0;
	};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	ibl::ExamplePBR_IBL
	, "43-PBR_IBL"
	, "PBR_IBL."
	, "https://bkaradzic.github.io/bgfx/examples.html#tess"
	);
