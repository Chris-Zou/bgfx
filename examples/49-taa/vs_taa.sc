$input a_position, a_texcoord
$output v_cs_pos, v_ss_txc

#include "../comon/common.sh"
#include "depthLibs.sh"
#include "NoiseLibs.sh"

void main()
{
	v_cs_pos = mul(u_modelViewProj, a_position);
	v_ss_txc = a_texcoord;
}
