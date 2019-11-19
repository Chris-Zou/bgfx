#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/ext.hpp>

#include "forward_shading_common.h"
#include "demos.h"


enum
{
	eModelCount = 1,
	eMaxLightCount = 2,
};

static Dolphin::Model s_model[eModelCount];

namespace Dolphin
{
	namespace SponzaDemo
	{
		void init()
		{
			s_model[0].loadModel("meshes/morgan-sponza.bin");
		}

		RenderList renderListScene()
		{
			RenderList rlist;
			rlist.models = &s_model[0];
			rlist.count = eModelCount;

			return rlist;
		}

		void shutdown()
		{
			for (int i = 0; i < eModelCount; ++i)
			{
				s_model[i].unload();
			}
		}
	}
}
