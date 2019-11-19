#include "bgfx_compute.sh"

#define GROUP_SIZE 64
#define THREADS_X 8
#define THREADS_Y 8

uniform vec4 u_params;

#define nearZ u_params.z
#define farZ u_params.w

uniform mat4 u_projection;

#if INITIAL_PASS
SAMPLER2D(s_inputDepthMap, 0);
#else
IMAGE2D_RO(s_inputDepthMap, rg16f, 0);
#endif

IMAGE2D_WR(s_output, rg16f, 1);

SHARED vec2 depthShared[GROUP_SIZE];

NUM_THREADS(THREADS_X, THREADS_Y, 1)
void main()
{
	
}
