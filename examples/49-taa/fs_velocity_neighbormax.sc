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
	const vec2 du = vec2(velocityTexel.x, 0.0);
	const vec2 dv = vec2(0.0, velocityTexel.y);

	vec2 mv = vec2(0.0, 0.0);
	vec2 dmv = vec2(0.0, 0.0);

	for(int i = -1; i <= 1; ++i)
	{
		for(int j = -1; j <= 1; ++j)
		{
			vec2 v = texture2D(s_velocityBuffer, v_ss_txc + i * dv + j * du).xy;
			float dv = dot(v, v);
			if(dv > dmv)
			{
				mv = v;
				dmv = dv;
			}
		}
	}

	gl_FragColor = vec4(mv, 0.0, 0.0);
}
