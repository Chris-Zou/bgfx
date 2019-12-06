$input a_position, a_texcoord0
$output v_cs_pos, v_ss_tex, v_ray

#include "../common/common.sh"

void main()
{
	v_cs_pos = mul(u_modelViewProj, vec4(a_position, 1.0));
	v_ss_tex = a_texcoord0;
	v_ray = 2.0 * a_texcoord0 - 1.0;
	gl_Position = v_cs_pos;
}
