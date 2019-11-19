$input a_position, a_normal, a_texcoord0, a_tangent
$output v_texcoord

#include "../common/common.sh"

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
	v_texcoord = a_texcoord0;
}
