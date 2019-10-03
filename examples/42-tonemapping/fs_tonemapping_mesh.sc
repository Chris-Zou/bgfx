$input v_pos, v_view, v_normal

#include "../common/common.sh"

uniform vec4 u_params;
#define u_time u_tonemap.w

SAMPLERCUBE(s_texCube, 0);

vec2 blinn(vec3 lightDir, vec3 normal, vec3 viewDir)
{
	float ndotl = dot(normal, lightDir);
	vec3 reflected = lightDir - 2.0 * ndotl * normal;
	float rdotv = dot(reflected, viewDir);

	return vec2(ndotl, rdotv);
}

float fresnel(float ndotl, float bias, float _pow)
{
	float facing = (1.0 - ndotl);
	return max(bias + (1.0 - bias) * pow(facing, _pow), 0.0);
}

vec4 lit(float ndotl, float rdotv, float m)
{
	float diff = max(0.0, ndotl);
	float spec = step(0.0, ndotl) * max(0.0, rdotv * m);

	return vec4(1.0, diff, spec, 1.0);
}

void main()
{
	vec3 lightDir = vec3(0.0, 0.0, -1.0);
	vec3 normal = normalize(v_normal);
	vec3 view = normalize(v_view);
	vec2 bln = blinn(lightDir, normal, view);
	vec4 lc = lit(bln.x, bln.y, 1.0);
	float fres = fresnel(bln.x, 0.2, 5.0);

	vec3 color = vec3(1.0, 1.0, 1.0);

	color *= textureCube(s_texCube, reflect(view, -normal)).xyz;

	gl_FragColor = vec4(color.xyz * lc.y + fres * pow(lc.z, 128.0), 1.0);
}
