$input v_cs_pos, v_ss_tex, v_ray

#include "../common/common.sh"

SAMPLER2D(s_depthBuffer, 0);
uniform vec4 texelSize;

#include "depth_libs.sh"

uniform vec4 u_params;
uniform mat4 u_prevV;
uniform mat4 u_prevP;
uniform mat4 u_invPrevV;
uniform mat4 u_invPrevP;

#define nearPlane u_params.x
#define farPlane u_params.y

/*
https://stackoverflow.com/questions/32227283/getting-world-position-from-depth-buffer-value

vec3 WorldPosFromDepth(float depth) {
    float z = depth * 2.0 - 1.0;

    vec4 clipSpacePosition = vec4(TexCoord * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    vec4 worldSpacePosition = viewMatrixInv * viewSpacePosition;

    return worldSpacePosition.xyz;
}

*/

void main()
{
	float depth = texture2D(s_depthBuffer, v_ss_tex);

#if BGFX_SHADER_LANGUAGE_GLSL
	vec4 clip_pos = vec4(v_ss_tex * 2.0 - 1.0, 2.0 * depth - 1.0, 1.0);
#else
	vec4 clip_pos = vec4(vec2(v_ss_tex.x, 1.0 - v_ss_tex.y) * 2.0 - 1.0, depth, 1.0);
#endif

	mat4 mat_vp = mul(u_invPrevV, u_invPrevP);
	vec4 vs_pos = mul(mat_vp, clip_pos);
	vs_pos /= vs_pos.w;

	vec4 ws_pos = vs_pos;

	mat4 prevVP = mul(u_prevP, u_prevV);

	vec4 rp_cs_pos = mul(prevVP, ws_pos);
	vec2 rp_ss_ndc = rp_cs_pos.xy / rp_cs_pos.w;
	vec2 rp_ss_tex = 0.5 * rp_ss_ndc + 0.5;
	rp_ss_tex.y = 1.0 - rp_ss_tex.y;
	vec2 ss_vel = v_ss_tex - rp_ss_tex;

	gl_FragColor = vec4(ss_vel, 0.0, 0.0);
}
