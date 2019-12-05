$input a_position, a_texcoord0
$output v_cs_pos, v_ss_txc, v_ray

#include "../common/common.sh"

void main()
{
	v_cs_pos = mul(u_modelViewProj, a_position);
	v_ss_txc = a_texcoord0;
	v_ray = 2.0 * a_texcoord0 - 1.0;
}
