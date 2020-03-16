$input v_texcoord0

#include "../common/common.sh"

SAMPLER2D(s_color, 0);
SAMPLER2D(s_normal, 1);
SAMPLER2D(s_ao, 2);

uniform vec4 u_combineParams[2];

void main()
{
	vec2 tc0 = v_texcoord0 * u_combineParams[1].xy + u_combineParams[1].zw;
	vec3 albedoColor = vec3(1.0f, 1.0f, 1.0f);
	if(u_combineParams[0].x > 0.0f)
	{
		albedoColor = texture2D(s_color, tc0).rgb;
	}

	float light = 1.0f;
	if(u_combineParams[0].x > 0.0f)
	{
		vec3 n = texture2D(s_normal, tc0).xyz;
		n = n * 2.0f - 1.0f;
		vec3 l = normalize(vec3(-0.8f, 0.75f, -1.0f));
		light = max(0.0f, dot(n, l)) * 1.2f + 0.3f;
	}

	float ao = 1.0f;
	if(u_combineParams[0].y > 0.0f)
	{
		ao = texture2D(s_ao, tc0);
	}

	gl_FragColor = vec4(albedoColor * light * ao, 1.0f);
}
