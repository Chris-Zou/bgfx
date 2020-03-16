#include "bgfx_compute.sh" 
#include "uniforms.sh"

BUFFER_WR(s_loadCounter, uint, 0);

NUM_THREADS(1, 1, 1)
void main() 
{
	s_loadCounter[0] = 0;
}
