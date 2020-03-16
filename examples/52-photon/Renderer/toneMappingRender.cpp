#include "toneMappingRender.h"

namespace Dolphin
{
	bgfx::VertexLayout ToneMapping::ScreenSpaceQuadVertex::ms_layout;
	bool Dolphin::ToneMapping::ScreenSpaceQuadVertex::isInitialized = false;

	bgfx::ViewId ToneMapping::render(bgfx::TextureHandle hdrFbTexture, const ToneMapParams& toneMapParams, const float deltaTime, bgfx::ViewId startingPass)
	{
		bgfx::ViewId histogramPass = startingPass;
		bgfx::ViewId averagingPass = startingPass + 1;
		bgfx::ViewId toneMapPass = startingPass + 2;

		bgfx::setViewName(histogramPass, "Luminence Histogram");
		bgfx::setViewName(averagingPass, "Avergaing the Luminence Histogram");

		bgfx::setViewName(toneMapPass, "Tonemap");
		bgfx::setViewRect(toneMapPass, 0, 0, bgfx::BackbufferRatio::Equal);
		bgfx::setViewFrameBuffer(toneMapPass, BGFX_INVALID_HANDLE);

		bgfx::setViewTransform(toneMapPass, nullptr, orthoProjection);

		float logLumRange = toneMapParams.m_maxLogLuminance - toneMapParams.m_minLogLuminance;
		float histogramParams[4] = {
				toneMapParams.m_minLogLuminance,
				1.0f / (logLumRange),
				float(toneMapParams.m_width),
				float(toneMapParams.m_height),
		};
		uint32_t groupsX = static_cast<uint32_t>(bx::ceil(toneMapParams.m_width / 16.0f));
		uint32_t groupsY = static_cast<uint32_t>(bx::ceil(toneMapParams.m_height / 16.0f));
		bgfx::setUniform(paramsUniform, histogramParams);
		bgfx::setImage(0, hdrFbTexture, 0, bgfx::Access::Read, frameBufferFormat);
		bgfx::setBuffer(1, histogramBuffer, bgfx::Access::Write);
		bgfx::dispatch(histogramPass, histogramProgram, groupsX, groupsY, 1);

		float timeCoeff = bx::clamp<float>(1.0f - bx::exp(-deltaTime * toneMapParams.m_tau), 0.0, 1.0);
		float avgParams[4] = {
				toneMapParams.m_minLogLuminance,
				logLumRange,
				timeCoeff,
				static_cast<float>(toneMapParams.m_width * toneMapParams.m_height),
		};
		bgfx::setUniform(paramsUniform, avgParams);
		bgfx::setImage(0, avgLuminanceTarget, 0, bgfx::Access::ReadWrite, bgfx::TextureFormat::R16F);
		bgfx::setBuffer(1, histogramBuffer, bgfx::Access::ReadWrite);
		bgfx::dispatch(averagingPass, averagingProgram, 1, 1, 1);

		bgfx::setTexture(0, s_hdrTexture, hdrFbTexture, BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
		bgfx::setTexture(1, s_texAvgLuminance, avgLuminanceTarget, BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		setScreenSpaceQuad(float(toneMapParams.m_width), float(toneMapParams.m_height), toneMapParams.m_originBottomLeft);
		bgfx::submit(toneMapPass, tonemappingProgram);

		return toneMapPass + 1;
	}
}
