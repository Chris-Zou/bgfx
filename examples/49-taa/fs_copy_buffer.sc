$input v_texcoord

#include "../common/common.sh"

SAMPLER2D(s_historyBuffer, 0);

void main()
{
	gl_FragColor = texture2D(s_historyBuffer, v_texcoord);
}