$input a_position, a_texcoord0
$output v_texcoord, v_position

#include "../common/common.sh"

void main()
{
	v_texcoord = a_texcoord0;
	vec4 pos = mul(u_invViewProj, vec4(a_position, 1.0f));
	v_position = pos.xyz / pos.w;
	gl_Position = vec4(a_position, 1.0);
}
