$input v_cs_pos, v_ss_tex, v_ray

#include "../common/common.sh"

SAMPLER2D(s_depthBuffer, 0);
uniform vec4 texelSize;

#include "depth_libs.sh"

uniform vec4 u_params;
uniform mat4 u_prevVP;

#define nearPlane u_params.x
#define farPlane u_params.y

void main()
{
	float vs_dist = depth_sample_linear(v_ss_tex, nearPlane, farPlane);
	vec3 vs_pos = float3(v_ray, 1.0) * vs_dist;
	
	vec4 ws_pos = mul(u_model[0], vec4(vs_pos, 1.0));

	mat4 prevVP = u_prevVP * u_proj;
	vec4 rp_cs_pos = mul(prevVP, ws_pos);
	vec2 rp_ss_ndc = rp_cs_pos.xy / rp_cs_pos.w;
	vec2 rp_ss_tex = 0.5 * rp_ss_ndc + 0.5;

	vec2 ss_vel = v_ss_tex - rp_ss_tex;

	gl_FragColor = vec4(ss_vel, 0.0, 0.0);
}
