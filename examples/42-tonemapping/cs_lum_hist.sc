#include <bgfx_compute.sh>

#define GROUP_SIZE 256
#define THREADS_X 16
#define THREADS_Y 16

#define EPSILON 0.000001
#define RGB_TO_LUM vec3(0.2125, 0.7154, 0.0721)

uniform vec4 u_params;

IMAGE2D_RO(s_texColor, rgba16f, 0);
BUFFER_RW(histogram, uint, 1);

SHARED uint histogramShared[GROUP_SIZE];

uint colorToBin(vec3 hdrColor, float minLogLum, float inverseLogLumRange)
{
	float lum = dot(hdrColor, RGB_TO_LUM);

	if(lum < EPSILON)
	{
		return 0;
	}

	float logLum = clamp((log2(lum) - minLogLum) * inverseLogLumRange, 0.0f, 1.0f);
	return uint(logLum * 254.0f + 1.0f);
}

NUM_THREADS(THREADS_X, THREADS_Y, 1)
void main()
{
	histogramShared[gl_LocalInvocationIndex] = 0;
	groupMemoryBarrier();

	uvec2 dim = uvec2(u_params.zw);
	if(gl_GlobalInvocationID.x < dim.x && gl_GlobalInvocationID.y < dim.y)
	{
		vec3 hdrColor = imageLoad(s_texColor, ivec2(gl_GlobalInvocationID.xy)).rgb;
		uint binIndex = colorToBin(hdrColor, u_params.x, u_params.y);
		atomicAdd(histogramShared[binIndex], 1);
	}

	groupMemoryBarrier();

	atomicAdd(histogram[gl_LocalInvocationIndex], histogramShared[gl_LocalInvocationIndex]);
}
