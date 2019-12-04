$input v_texcoord

#include "../common/common.sh"

SAMPLER2D(s_depthBuffer, 0);
uniform vec4 u_params;

#define nearPlane u_params.x
#define farPlane u_params.y
#define texelSize u_params.zw

/*
n = near
f = far
z = depth buffer Z-value
EZ  = eye Z value
LZ  = depth buffer Z-value remapped to a linear [0..1] range (near plane to far plane)
LZ2 = depth buffer Z-value remapped to a linear [0..1] range (eye to far plane)


DX:
EZ  = (n * f) / (f - z * (f - n))
LZ  = (eyeZ - n) / (f - n) = z / (f - z * (f - n))
LZ2 = eyeZ / f = n / (f - z * (f - n))


GL:
EZ  = (2 * n * f) / (f + n - z * (f - n))
LZ  = (eyeZ - n) / (f - n) = n * (z + 1.0) / (f + n - z * (f - n))
LZ2 = eyeZ / f = (2 * n) / (f + n - z * (f - n))



LZ2 in two instructions:
LZ2 = 1.0 / (c0 * z + c1)

DX:
  c1 = f / n
  c0 = 1.0 - c1

GL:
  c0 = (1 - f / n) / 2
  c1 = (1 + f / n) / 2


-------------------
http://www.humus.ca
*/

float resolve_linear_depth(float z)
{
#if BGFX_SHADER_LANGUAGE_GLSL
	(2.0 * nearPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
#else
	nearPlane / (farPlane - z * (farPlane - nearPlane));
#endif
}

float depth_sample_linear(float2 uv)
{
	return resolve_linear_depth(texture2D(s_depthBuffer, uv).x);
}

float3 find_closet_fragment_3x3(float2 uv)
{
	float2 dd = abs(texelSize.xy);
	float2 du = float2(dd.x, 0.0);
	float2 dv = float2(0.0, dd.y);

	float3 dtl = float3(-1, -1, texture2D(s_depthBuffer, uv - dv - du).x);
	float3 dtc = float3(0, -1, texture2D(s_depthBuffer, uv - dv).x);
	float3 dtr = float3(1, -1, texture2D(s_depthBuffer, uv - dv + du).x);

	float3 dml = float3(-1, 0, texture2D(s_depthBuffer, uv - du).x);
	float3 dmc = float3(0, 0, texture2D(s_depthBuffer, uv).x);
	float3 dmr = float3(1, 0, texture2D(s_depthBuffer, uv + du).x);

	float3 dbl = float3(-1, 1, texture2D(s_depthBuffer, uv + dv - du).x);
	float3 dbc = float3(0, 1, texture2D(s_depthBuffer, uv + dv).x);
	float3 dbr = float3(1, 1, texture2D(s_depthBuffer, uv + dv + du).x);
}

void main()
{
	
}
