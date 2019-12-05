$input v_cs_pos, v_ss_txc, v_ray

#include "../common/common.sh"
#include "depthLibs.sh"

uniform vec4 u_params;
uniform mat4 u_prevVP;

#define nearPlane u_params.x
#define farPlane u_params.y

void main()
{
	float vs_dist = depth_sample_linear(v_ss_txc);
	vec3 vs_pos = float3(v_vs_ray, 1.0) * vs_dist;

	vec4 ws_pos = mul(u_model[0], vec4(vs_pos, 1.0));

	vec4 rp_cs_pos = mul(u_prevVP, ws_pos);
	vec2 rp_ss_ndc = rp_cs_pos.xy / rp_cs_pos.w;
	vec2 rp_ss_txc = 0.5 * rp_ss_ndc + 0.5;

	vec2 ss_vel = v_ss_txc - rp_ss_txc;

	gl_FragColor = vec4(ss_vel, 0.0, 0.0);
}
