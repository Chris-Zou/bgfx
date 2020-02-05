$input v_normal, v_texcoord0

#include "../common/common.sh"

SAMPLER2D(s_albedo, 0);

void main()
{
	vec3 normalWorldSpace = v_normal;

	gl_FragData[0].xyz = normalWorldSpace.xyz;
	gl_FragData[0].w = 0.0;

	gl_FragData[1] = texture2D(s_albedo, v_texcoord0);
}
