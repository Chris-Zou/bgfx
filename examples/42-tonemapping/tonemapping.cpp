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
#include <bx\rng.h>

namespace
{
#define SAMPLER_POINT_CLAMP  (BGFX_SAMPLER_POINT|BGFX_SAMPLER_UVW_CLAMP)

	static float s_texelHalf = 0.0f;

	static const char* s_operatorNames[]
	{
		"Reinhard",
		"Lottes",
		"Uchimura",
		"Unreal"
	};


	struct PosColorTexCoord0Vertex
	{
		float m_x;
		float m_y;
		float m_z;
		uint32_t m_rgba;
		float m_u;
		float m_v;

		static void init()
		{
			ms_layout
				.begin()
				.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
				.end();
		}

		static bgfx::VertexLayout ms_layout;
	};

	bgfx::VertexLayout PosColorTexCoord0Vertex::ms_layout;

	void screenSpaceQuad(float _textureWidth, float _textureHeight, bool _originBottomLeft = false, float _width = 1.0f, float _height = 1.0f)
	{
		if (3 == bgfx::getAvailTransientVertexBuffer(3, PosColorTexCoord0Vertex::ms_layout))
		{
			bgfx::TransientVertexBuffer vb;
			bgfx::allocTransientVertexBuffer(&vb, 3, PosColorTexCoord0Vertex::ms_layout);
			PosColorTexCoord0Vertex* vertex = (PosColorTexCoord0Vertex*)vb.data;

			const float zz = 0.0f;

			const float minx = -_width;
			const float maxx = _width;
			const float miny = 0.0f;
			const float maxy = _height * 2.0f;

			const float texelHalfW = s_texelHalf / _textureWidth;
			const float texelHalfH = s_texelHalf / _textureHeight;
			const float minu = -1.0f + texelHalfW;
			const float maxu = 1.0f + texelHalfW;

			float minv = texelHalfH;
			float maxv = 2.0f + texelHalfH;

			if (_originBottomLeft)
			{
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

	class ExampleToneMapping : public entry::AppI
	{
	public:
		ExampleToneMapping(const char* _name, const char* _description, const char* _url)
			: entry::AppI(_name, _description, _url)
		{
		}

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
			/*m_skyProgram = compileShader("../42-tonemapping/vs_tonemapping_skybox.sc", "../42-tonemapping/fs_tonemapping_skybox.sc", "../42-tonemapping/varying.def.sc");
			m_meshProgram = compileShader("../42-tonemapping/vs_tonemapping_mesh.sc", "../42-tonemapping/fs_tonemapping_mesh.sc", "../42-tonemapping/varying.def.sc");*/

			m_histogramProgram = compileComputeShader("../42-tonemapping/cs_lum_hist.sc");
			m_avgProgram = compileComputeShader("../42-tonemapping/cs_lum_avg.sc");

			m_tonemapPrograms[0] = compileShader("../42-tonemapping/vs_tonemapping_tonemap.sc", "../42-tonemapping/fs_reinhard.sc", "../42-tonemapping/varying.def.sc");
			m_tonemapPrograms[1] = compileShader("../42-tonemapping/vs_tonemapping_tonemap.sc", "../42-tonemapping/fs_lottes.sc", "../42-tonemapping/varying.def.sc");
			m_tonemapPrograms[2] = compileShader("../42-tonemapping/vs_tonemapping_tonemap.sc", "../42-tonemapping/fs_uchimura.sc", "../42-tonemapping/varying.def.sc");
			m_tonemapPrograms[3] = compileShader("../42-tonemapping/vs_tonemapping_tonemap.sc", "../42-tonemapping/fs_unreal.sc", "../42-tonemapping/varying.def.sc");
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

			const bgfx::Caps* caps = bgfx::getCaps();
			m_computeSupported = !!(caps->supported & BGFX_CAPS_COMPUTE);

			if (!m_computeSupported)
				return;

			PosColorTexCoord0Vertex::init();

			m_envTexture = loadTexture("textures/pisa_with_mips.ktx", 0 | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP| BGFX_SAMPLER_W_CLAMP);

			s_texCube = bgfx::createUniform("s_texCube", bgfx::UniformType::Sampler);
			s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
			s_texAvgLum = bgfx::createUniform("s_texAvgLum", bgfx::UniformType::Sampler);
			u_mtx = bgfx::createUniform("u_mtx", bgfx::UniformType::Mat4);
			u_tonemap = bgfx::createUniform("u_tonemap", bgfx::UniformType::Vec4);
			u_histogramParams = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);

			if (m_firstFrame)
			{
				compileShaders();
			}

			m_mesh = meshLoad("meshes/bunny.bin");

			m_fbh.idx = bgfx::kInvalidHandle;

			m_histogramBuffer = bgfx::createDynamicIndexBuffer(256, BGFX_BUFFER_COMPUTE_READ_WRITE | BGFX_BUFFER_INDEX32);

			imguiCreate();

			m_caps = bgfx::getCaps();
			s_texelHalf = bgfx::RendererType::Direct3D9 == m_caps->rendererType ? 0.5f : 0.0f;

			m_oldWidth = 0;
			m_oldHeight = 0;
			m_oldReset = m_reset;

			m_speed = 0.37;
			m_white = 3.0f;
			m_threshold = 1.5f;

			m_time = 0.0f;
		}

		virtual int shutdown() override
		{
			imguiDestroy();

			if (!m_computeSupported) {
				return 0;
			}

			meshUnload(m_mesh);

			if (bgfx::isValid(m_fbh))
			{
				bgfx::destroy(m_fbh);
			}

			bgfx::destroy(m_meshProgram);
			bgfx::destroy(m_skyProgram);
			for (size_t i = 0; i < BX_COUNTOF(m_tonemapPrograms); ++i) {
				bgfx::destroy(m_tonemapPrograms[i]);
			}
			bgfx::destroy(m_histogramProgram);
			bgfx::destroy(m_avgProgram);

			bgfx::destroy(m_envTexture);

			bgfx::destroy(m_histogramBuffer);
			bgfx::destroy(m_lumAvgTarget);

			bgfx::destroy(s_texCube);
			bgfx::destroy(s_texColor);
			bgfx::destroy(s_texAvgLum);
			bgfx::destroy(u_mtx);
			bgfx::destroy(u_tonemap);
			bgfx::destroy(u_histogramParams);

			// Shutdown bgfx.
			bgfx::shutdown();

			return 0;
		}

		bool update() override
		{
			if (!entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState))
			{
				if (!m_computeSupported)
					return false;

				if (!bgfx::isValid(m_fbh) || m_oldWidth != m_width || m_oldHeight != m_height || m_oldReset == m_reset)
				{
					m_oldWidth = m_width;
					m_oldHeight = m_height;
					m_oldReset = m_reset;

					uint32_t msaa = (m_reset & BGFX_RESET_MSAA_MASK) >> BGFX_RESET_MSAA_SHIFT;

					if (bgfx::isValid(m_fbh))
					{
						bgfx::destroy(m_fbh);
					}

					m_fbTextures[0] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::RGBA16F, (uint64_t(msaa + 1) << BGFX_TEXTURE_RT_MSAA_SHIFT) | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

					const uint64_t textureFlags = BGFX_TEXTURE_RT_WRITE_ONLY | (uint64_t(msaa + 1) << BGFX_TEXTURE_RT_MSAA_SHIFT);

					bgfx::TextureFormat::Enum depthFormat =
						bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D16, textureFlags) ? bgfx::TextureFormat::D16
						: bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D24S8, textureFlags) ? bgfx::TextureFormat::D24S8
						: bgfx::TextureFormat::D32;

					m_fbTextures[1] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, depthFormat, textureFlags);

					m_fbh = bgfx::createFrameBuffer(BX_COUNTOF(m_fbTextures), m_fbTextures, true);

					uint64_t lumAvgFlags = BGFX_TEXTURE_COMPUTE_WRITE | SAMPLER_POINT_CLAMP;
					m_lumAvgTarget = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::R16F, lumAvgFlags);
					bgfx::setName(m_lumAvgTarget, "LumAvgTarget");
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
					ImVec2(m_width / 5.0f, m_height / 2.0f)
					, ImGuiCond_FirstUseEver
				);
				ImGui::Begin("Settings"
					, NULL
					, 0
				);

				ImGui::SliderFloat("Speed", &m_speed, 0.0f, 1.0f);
				ImGui::Separator();

				ImGui::Text("Tone Mapping Operator");
				ImGui::Combo("", (int*)&m_currentOperator, s_operatorNames, BX_COUNTOF(s_operatorNames));

				if (m_currentOperator == 0) {
					ImGui::SliderFloat("White Point", &m_white, 0.1f, 5.0f);
				}

				ImGui::End();

				imguiEndFrame();

				bgfx::touch(0);


				int64_t now = bx::getHPCounter();
				static int64_t last = now;
				const int64_t frameTime = last - now;
				last = now;
				const double freq = double(bx::getHPFrequency());

				m_time += (float)(frameTime * m_speed / freq);

				bgfx::ViewId hdrSkybox = 0;
				bgfx::ViewId hdrMesh = 1;
				bgfx::ViewId histogramPass = 2;
				bgfx::ViewId averagingPass = 3;
				bgfx::ViewId toneMapPass = 4;

				bgfx::setViewName(hdrSkybox, "Skybox");
				bgfx::setViewClear(hdrSkybox, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
				bgfx::setViewRect(hdrSkybox, 0, 0, bgfx::BackbufferRatio::Equal);
				bgfx::setViewFrameBuffer(hdrSkybox, m_fbh);

				bgfx::setViewName(hdrMesh, "Mesh");
				bgfx::setViewClear(hdrMesh, BGFX_CLEAR_DISCARD_DEPTH | BGFX_CLEAR_DISCARD_STENCIL);
				bgfx::setViewRect(hdrMesh, 0, 0, bgfx::BackbufferRatio::Equal);
				bgfx::setViewFrameBuffer(hdrMesh, m_fbh);

				bgfx::setViewName(histogramPass, "Luminence Histogram");

				bgfx::setViewName(averagingPass, "Averaging the Luminence Histogram");

				bgfx::setViewName(toneMapPass, "Tonemap");
				bgfx::setViewRect(toneMapPass, 0, 0, bgfx::BackbufferRatio::Equal);
				bgfx::FrameBufferHandle invalid = BGFX_INVALID_HANDLE;
				bgfx::setViewFrameBuffer(toneMapPass, invalid);

				const bgfx::Caps* caps = bgfx::getCaps();
				float proj[16];
				bx::mtxOrtho(proj, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f, 0.0f, caps->homogeneousDepth);

				for (uint8_t i = 0; i < toneMapPass; ++i)
				{
					bgfx::setViewTransform(i, nullptr, proj);
				}

				const bx::Vec3 at = { 0.0f, 1.0f, 0.0f };
				const bx::Vec3 eye = { 0.0f, 1.0f, -2.5f };

				float mtx[16];
				bx::mtxRotateXY(mtx, 0.0f, m_time);

				const bx::Vec3 tmp = bx::mul(eye, mtx);

				float view[16];
				bx::mtxLookAt(view, tmp, at);
				bx::mtxProj(proj, 60.0f, float(m_width) / float(m_height), 0.1f, 100.0f, caps->homogeneousDepth);

				bgfx::setTexture(0, s_texCube, m_envTexture);
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
				bgfx::setUniform(u_mtx, mtx);
				screenSpaceQuad((float)m_width, (float)m_height, true);
				bgfx::submit(hdrSkybox, m_skyProgram);

				bgfx::setViewTransform(hdrMesh, view, proj);
				bgfx::setTexture(0, s_texCube, m_envTexture);
				meshSubmit(m_mesh, hdrMesh, m_meshProgram, nullptr);

				float minLogLum = -8.0f;
				float maxLogLum = 3.5f;
				float histogramParams[4] = {
					minLogLum,
					1.0f / (maxLogLum - minLogLum),
					float(m_width),
					float(m_height)
				};

				uint32_t groupsX = static_cast<uint32_t>(bx::ceil(m_width / 16.0f));
				uint32_t groupsY = static_cast<uint32_t>(bx::ceil(m_height / 16.0f));
				bgfx::setImage(0, m_fbTextures[0], 0, bgfx::Access::Read, bgfx::TextureFormat::RGBA16F);
				bgfx::setBuffer(1, m_histogramBuffer, bgfx::Access::Write);
				bgfx::setUniform(u_histogramParams, histogramParams);
				bgfx::dispatch(histogramPass, m_histogramProgram, groupsX, groupsY, 1);

				float tau = 1.1f;
				float timeCoeff = bx::clamp<float>(1.0f - bx::exp(-frameTime * tau), 0.0f, 1.0f);
				float avgParams[4] = {
					minLogLum,
					maxLogLum - minLogLum,
					timeCoeff,
					static_cast<float>(m_width * m_height)
				};
				bgfx::setImage(0, m_lumAvgTarget, 0, bgfx::Access::ReadWrite, bgfx::TextureFormat::R16F);
				bgfx::setBuffer(1, m_histogramBuffer, bgfx::Access::ReadWrite);
				bgfx::setUniform(u_histogramParams, avgParams);
				bgfx::dispatch(averagingPass, m_avgProgram, 1, 1, 1);

				float tonemap[4] = { bx::square(static_cast<float>(m_width)), 0.0f, m_threshold, m_time };
				bgfx::setTexture(0, s_texColor, m_fbTextures[0]);
				bgfx::setTexture(1, s_texAvgLum, m_lumAvgTarget, SAMPLER_POINT_CLAMP);
				bgfx::setUniform(u_tonemap, tonemap);
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
				screenSpaceQuad(float(m_width), float(m_height), m_caps->originBottomLeft);
				bgfx::submit(toneMapPass, m_tonemapPrograms[m_currentOperator]);

				bgfx::frame();

				m_firstFrame = false;

				return true;
			}

			return false;
		}

	public :
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_debug;
		uint32_t m_reset;

		entry::MouseState m_mouseState;

		bgfx::ProgramHandle m_skyProgram;
		bgfx::ProgramHandle m_meshProgram;
		bgfx::ProgramHandle m_tonemapPrograms[4];
		bgfx::ProgramHandle m_histogramProgram;
		bgfx::ProgramHandle m_avgProgram;

		bgfx::TextureHandle m_envTexture;
		bgfx::UniformHandle s_texColor;
		bgfx::UniformHandle s_texCube;
		bgfx::UniformHandle s_texAvgLum;
		bgfx::UniformHandle u_mtx;
		bgfx::UniformHandle u_tonemap;
		bgfx::UniformHandle u_histogramParams;

		Mesh* m_mesh;

		bgfx::DynamicIndexBufferHandle m_histogramBuffer;

		bgfx::TextureHandle m_fbTextures[2];
		bgfx::TextureHandle m_lumAvgTarget;
		bgfx::FrameBufferHandle m_fbh;

		bx::RngMwc m_rng;

		uint32_t m_oldWidth;
		uint32_t m_oldHeight;
		uint32_t m_oldReset;

		int32_t m_currentOperator = 0;

		float m_speed;
		float m_white;
		float m_threshold;

		const bgfx::Caps* m_caps;
		float m_time;

		bool m_computeSupported;
		bool m_firstFrame;
	};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	ExampleToneMapping
	, "42-tonemapping"
	, "tonemapping."
	, "https://bkaradzic.github.io/bgfx/examples.html#tess"
	);
