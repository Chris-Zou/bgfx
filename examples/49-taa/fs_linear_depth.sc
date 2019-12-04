$input v_texcoord

#include "../common/common.sh"
#include ""

SAMPLER2D(s_depthBuffer, 0);
uniform vec4 u_params;

#define nearPlane u_params.x
#define farPlane u_params.y
#define texelSize u_params.zw

void main()
{
	
}
