$input v_texcoord0

#include "../common/common.sh"

SAMPLERCUBE(s_texCube, 0);
uniform mat4 u_mtx;

void main()
{
	vec3 dir = vec3(v_texcoord0 * 2.0 - 1.0, 1.0);
	dir = normalize(mul(u_mtx, vec4(dir, 0.0)).xyz);
	dir.y *= -1.0;
	gl_FragColor = vec4(textureCube(s_texCube, dir).xyz, 1.0);
}
