$input v_cs_pos, v_ss_txc, v_ray

#include "../common/common.sh"
#include "depthLibs.sh"

uniform vec4 u_params;

#define nearPlane u_params.x
#define farPlane u_params.y

void main()
{
	float vs_dist = depth_sample_linear(v_ss_txc);
	vec3 vs_pos = float3(v_vs_ray, 1.0) * vs_dist;
}
