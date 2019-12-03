$input v_texcoord0

#include "../common/common.sh"

SAMPLER2D(s_historyBuffer, 0);

void main()
{
	gl_FragColor = texture2D(s_historyBuffer, v_texcoord0);
}
