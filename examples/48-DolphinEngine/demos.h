#ifndef __DEMOS_H__
#define __DEMOS_H__

#include <stdint.h>

namespace Dolphin
{
	struct RenderList
	{
		struct Model* models;
		uint64_t count;
	};

	namespace SponzaDemo
	{
		void init();
		void shutdown();

		RenderList renderListScene();
		RenderList renderListLights();
		struct LightData* lightSettings();
	}
}
#endif
