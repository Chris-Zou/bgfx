$input v_cs_pos, v_ss_txc, v_ray

#include "../common/common.sh"
#include "depthLibs.sh"

uniform vec4 u_params;
uniform mat4 u_prevVP;

SAMPLER2D(s_velocityBuffer, 0);

#define nearPlane u_params.x
#define farPlane u_params.y
#define velocityTexel u_params.zw

void main()
{
	const int support = 1;

	const vec2 step = velocityTexel;
	const vec2 base = v_ss_txc + (0.5 - 0.5 * support) * step;
	const vec2 du = vec2(velocityTexel.x, 0.0);
	const vec2 dv = vec2(0.0, velocityTexel.y);

	vec2 mv = vec2(0.0, 0.0);
	vec2 rmv = vec2(0.0, 0.0);

	for(int i = 0; i != support; ++i)
	{
		for(int j = 0; j != support; ++j)
		{
			vec2 v = texture2D(s_velocityBuffer, base + i * dv + j * du).xy;
			float rv = dot(v, v);
			if(rv > rmv)
			{
				mv = v;
				rmv = rv;
			}
		}
	}

	gl_FragColor = vec4(mv, 0.0, 0.0);
}
