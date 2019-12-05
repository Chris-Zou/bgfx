$input a_position, a_texcoord0
$output v_cs_pos, v_ss_pos, v_cs_xy_curr, v_cs_xy_prev

#include "../common/common.sh"

uniform mat4 currVP;
uniform mat4 prevVP;

vec4 getScreenPos(vec4 cs_pos)
{
	return cs_pos;
}

void main()
{
	float occlusion_bias = 0.03f;
	
	v_cs_pos = mul(u_modelViewProj, a_position);
	v_ss_pos = getScreenPos(v_cs_pos);
	v_ss_pos.z = -mul(mul(currV, CurrM), a_position).z - occlusion_bias;
	v_cs_xy_curr = v_cs_pos.xyw;
	v_cs_xy_prev = mul(mul(prevP, prevM), a_position).xyw;
}
