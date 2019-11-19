$input v_texcoord

#include "../common/common.sh"

void main()
{
	gl_FragColor = vec4(v_texcoord.x, v_texcoord.y, 1.0, 1.0);
}
