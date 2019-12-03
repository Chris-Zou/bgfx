#include <bx/allocator.h>
#include <bx/debug.h>
#include <bx/file.h>
#include <bx/math.h>
#include <bx/rng.h>

#include "bgfx_utils.h"
#include "imgui/imgui.h"

#include <vector>
#include <iostream>
#include <random>

#include "../ShaderCompiler/ShaderCompiler.h"

namespace Dolphin
{
	struct ToneMapParams
	{
		uint32_t m_width;
		uint32_t m_height;

		float m_minLogLuminance = -8.0f;
		float m_maxLogLuminance = 3.0f;

		float m_tau = 1.1f;
		bool m_originBottomLeft = false;
	};

	class ToneMapping
	{
	public:
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

		void init(const bgfx::Caps* caps)
		{
			histogramProgram = Dolphin::compileComputeShader("../42-tonemapping/cs_lum_hist.sc");
			averagingProgram = Dolphin::compileComputeShader("../42-tonemapping/cs_lum_avg.sc");

			tonemappingProgram = Dolphin::compileGraphicsShader("../42-tonemapping/vs_tonemapping_tonemap.sc", "../42-tonemapping/fs_unreal.sc", "../42-tonemapping/varying.def.sc");

			histogramBuffer = bgfx::createDynamicIndexBuffer(
				256,
				BGFX_BUFFER_COMPUTE_READ_WRITE | BGFX_BUFFER_INDEX32
			);

			uint64_t lumAvgFlags = BGFX_TEXTURE_COMPUTE_WRITE | BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP;
			avgLuminanceTarget = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::R16F, lumAvgFlags);
			bgfx::setName(avgLuminanceTarget, "Average Luminance Texture");

			paramsUniform = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);
			s_hdrTexture = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
			s_texAvgLuminance = bgfx::createUniform("s_texAvgLuminance", bgfx::UniformType::Sampler);

			ScreenSpaceQuadVertex::init();

			bx::mtxOrtho(orthoProjection, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 0.0f, caps->homogeneousDepth);
		}

		bgfx::ViewId render(bgfx::TextureHandle hdrFbTexture, const ToneMapParams& toneMapParams, const float deltaTime, bgfx::ViewId startingPass);

		void destroy()
		{
			bgfx::destroy(histogramProgram);
			bgfx::destroy(averagingProgram);
			bgfx::destroy(tonemappingProgram);
			bgfx::destroy(histogramBuffer);
			bgfx::destroy(avgLuminanceTarget);
			bgfx::destroy(paramsUniform);
			bgfx::destroy(s_hdrTexture);
			bgfx::destroy(s_texAvgLuminance);
		}

	public:
		bgfx::ProgramHandle histogramProgram = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle averagingProgram = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle tonemappingProgram = BGFX_INVALID_HANDLE;
		bgfx::DynamicIndexBufferHandle histogramBuffer = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle avgLuminanceTarget = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle paramsUniform = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_hdrTexture = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_texAvgLuminance = BGFX_INVALID_HANDLE;

		float orthoProjection[16];

		static constexpr bgfx::TextureFormat::Enum frameBufferFormat = bgfx::TextureFormat::RGBA16F;
	};
}
