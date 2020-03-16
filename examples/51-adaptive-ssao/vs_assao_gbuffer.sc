$input a_position, a_normal, a_texcoord0
$output v_normal, v_texcoord0

#include "../common/common.sh"

void main()
{
	vec3 pos = a_position.xyz;
	gl_Position = mul(u_modelViewProj, vec4(pos, 1.0));

	vec3 normalObjectSpace = a_normal.xyz * 2.0 - 1.0;

	vec3 normalWorldSpace = mul(u_model[0], vec4(normalObjectSpace, 0.0)).xyz;
	v_normal.xyz = normalize(normalWorldSpace) * 0.5 + 0.5;

	v_texcoord0 = a_texcoord0 * 16.0;
}
