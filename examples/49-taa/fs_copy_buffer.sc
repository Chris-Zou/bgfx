$input v_texcoord

#include "../common/common.sh"

SAMPLER2D(s_historyBuffer, 0);

void main()
{
	gl_FragColor = vec4(texture2D(s_historyBuffer, v_texcoord).xyz, 1.0);

	gl_FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
}
