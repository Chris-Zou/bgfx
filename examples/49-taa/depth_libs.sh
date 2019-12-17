#ifndef _DEPTH_LIBS_SH_
#define _DEPTH_LIBS_SH_

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

float resolve_linear_depth(float _nearPlane, float _farPlane, float z)
{
#if BGFX_SHADER_LANGUAGE_GLSL
	return (2.0 * _nearPlane) / (_farPlane + _nearPlane - z * (_farPlane - _nearPlane));
#else
	return _nearPlane / (_farPlane - z * (_farPlane - _nearPlane));
#endif
}

float depth_sample_linear(vec2 uv, float _nearPlane, float _farPlane)
{
	return resolve_linear_depth(_nearPlane, _farPlane, texture2D(s_depthBuffer, uv).x);
}

vec3 find_closet_fragment_3x3(vec2 uv, vec4 _texelSize)
{
	vec2 dd = abs(_texelSize.xy);
	vec2 du = vec2(dd.x, 0.0);
	vec2 dv = vec2(0.0, dd.y);

	vec3 dtl = vec3(-1, -1, texture2D(s_depthBuffer, uv - du - dv).x);
	vec3 dtc = vec3( 0, -1, texture2D(s_depthBuffer, uv      - dv).x);
	vec3 dtr = vec3( 1, -1, texture2D(s_depthBuffer, uv + du - dv).x);

	vec3 dml = vec3(-1,  0, texture2D(s_depthBuffer, uv - du     ).x);
	vec3 dmc = vec3( 0,  0, texture2D(s_depthBuffer, uv          ).x);
	vec3 dmr = vec3( 1,  0, texture2D(s_depthBuffer, uv + du     ).x);

	vec3 dbl = vec3(-1,  1, texture2D(s_depthBuffer, uv - du + dv).x);
	vec3 dbc = vec3( 0,  1, texture2D(s_depthBuffer, uv      + dv).x);
	vec3 dbr = vec3( 1,  1, texture2D(s_depthBuffer, uv + du + dv).x);

	vec3 dmin = dtl;
	if(dmin.z > dtc.z) dmin = dtc;
	if(dmin.z > dtr.z) dmin = dtr;
	if(dmin.z > dml.z) dmin = dml;
	if(dmin.z > dmc.z) dmin = dmc;
	if(dmin.z > dmr.z) dmin = dmr;
	if(dmin.z > dbl.z) dmin = dbl;
	if(dmin.z > dbc.z) dmin = dbc;
	if(dmin.z > dbr.z) dmin = dbr;

	return vec3(uv + dd.xy * dmin.xy, dmin.z);
}

#endif // _DEPTH_LIBS_SH_
