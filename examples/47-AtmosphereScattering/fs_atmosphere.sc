$input v_texcoord, v_position

#include "../common/common.sh"

#define PI 3.1415926

#if 0
uniform float PlanetRadius;
uniform float AtmosphereHeight;
uniform float SunIntensity;
uniform float DistanceScale;
#else
uniform vec4 u_params;
#define PlanetRadius u_params.x
#define AtmosphereHeight u_params.y
#define SunIntensity u_params.z
#define DistanceScale u_params.w
#endif

uniform vec2 DensityScaleHeight;

uniform vec3 ScatteringR;
uniform vec3 ScatteringM;
uniform vec3 ExtinctionR;
uniform vec3 ExtinctionM;

uniform vec4 IncomingLight;
uniform vec4 LightDir;

uniform vec3 CameraPos;

#if 0
uniform float MieG;
#else
#define MieG LightDir.w;
#endif

vec2 RaySphereIntersection(vec3 rayOrigin, vec3 rayDir, vec3 sphereCenter, float sphereRadius)
{
	rayOrigin -= sphereCenter;
	float a = dot(rayDir, rayDir);
	float b = 2.0 * dot(rayDir, rayOrigin);
	float c = dot(rayOrigin, rayOrigin) - (sphereRadius * sphereRadius);

	float d = b * b - 4.0 * a * c;
	if(d < 0.0)
	{
		return vec2(-1.0, -1.0);
	}
	else
	{
		d = sqrt(d);
		return vec2(-b -d, -b + d) / (2.0 * a);
	}
}

vec2 PrecomputeParticleDensity(vec3 rayStart, vec3 rayDir)
{
	vec3 planetCenter = vec3(0, -PlanetRadius, 0);

	float stepCount = 256;

	vec2 intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, PlanetRadius);
	if(intersection.x > 0)
	{
		return 1e+10;
	}

	intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, PlanetRadius + AtmosphereHeight);
	vec3 rayEnd = rayStart + rayDir * intersection.y;

	vec3 step = (rayEnd - rayStart) / stepCount;
	float stepSize = length(step);

	vec2 density = vec2(0.0, 0.0);

	for(float s = 0.5; s < stepCount; s += 1.0)
	{
		vec3 position = rayStart + step * s;
		float height = abs(length(position - planetCenter) - PlanetRadius);
		vec2 localDensity = exp(-(height.xx / DensityScaleHeight));

		density += localDensity * stepSize;
	}

	return density;
}

void GetAtmosphereDensity(vec3 position, vec3 planetCenter, vec3 lightDir, out vec2 localDensity, out vec2 densityToAtmosphereTop)
{
	float height = length(position - planetCenter) - PlanetRadius;
	localDensity = exp(-height.xx / DensityScaleHeight.xy);

	float cosAngle = dot(normalize(position - planetCenter), -lightDir.xyz);
	float sinAngle = sqrt(saturate(1.0 - cosAngle * cosAngle));
	vec3 rayDir = vec3(sinAngle, cosAngle, 0.0);
	vec3 rayStart = vec3(0.0, height, 0.0);

	densityToAtmosphereTop = PrecomputeParticleDensity(rayStart, rayDir);
}

void ComputeLocalInscattering(vec2 localDensity, vec2 densityPA, vec2 densityCP, out vec3 localInscatterR, out vec3 localInscatterM)
{
	vec2 densityCPA = densityCP + densityPA;

	vec3 Tr = densityCPA.x * ExtinctionR;
	vec3 Tm = densityCPA.y * ExtinctionM;

	vec3 extinction = exp(-(Tr + Tm));

	localInscatterR = localDensity.x * extinction;
	localInscatterM = localDensity.y * extinction;
}

float Sun(float cosAngle)
{
	float g = 0.98;
	float g2 = g * g;

	float sun = pow(1 - g, 2.0) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * cosAngle, 1.5));

	return sun * 0.003;
}

vec3 RenderSun(in vec3 scatterM, float cosAngle)
{
	return scatterM * Sun(cosAngle); 
}

void ApplyPhaseFunction(inout vec3 scatterR, inout vec3 scatterM, float cosAngle)
{
	float phase = (3.0 / (16.0 * PI)) * (1 + (cosAngle * cosAngle));
	scatterR *= phase;

	float g = MieG;
	float g2 = g * g;
	phase = (1.0 / (4.0 * PI)) * ((3.0 * (1.0 - g2)) / (2.0 * (2.0 + g2))) * ((1 + cosAngle * cosAngle) / (pow((1 + g2 - 2 * g*cosAngle), 3.0 / 2.0)));
	scatterM *= phase;
}

vec4 IntegrateInscattering(vec3 rayStart, vec3 rayDir, float rayLength, vec3 planetCenter, float distanceScale, vec3 lightDir, float sampleCount, out vec4 extinction)
{
	vec3 step = rayDir * (rayLength / sampleCount);
	float stepSize = length(step) * distanceScale;

	vec2 densityCP = 0;
	vec3 scatterR = 0;
	vec3 scatterM = 0;

	vec2 localDensity;
	vec2 densityPA;

	vec2 prevLocalDensity;
	vec3 prevLocalInscatterR, prevLocalInscatterM;
	GetAtmosphereDensity(rayStart, planetCenter, lightDir, prevLocalDensity, densityPA);
	ComputeLocalInscattering(prevLocalDensity, densityPA, densityCP, prevLocalInscatterR, prevLocalInscatterM);

	for(float s = 1.0; s < sampleCount; s += 1.0)
	{
		vec3 p = rayStart + step * s;

		GetAtmosphereDensity(p, planetCenter, lightDir, localDensity, densityPA);
		densityCP += (localDensity + prevLocalDensity) * (stepSize / 2.0);

		prevLocalDensity = localDensity;

		vec3 localInscatterR, localInscatterM;
		ComputeLocalInscattering(localDensity, densityPA, densityCP, localInscatterR, localInscatterM);

		scatterR += (localInscatterR + prevLocalInscatterR) * (stepSize / 2.0);
		scatterM += (localInscatterM + prevLocalInscatterM) * (stepSize / 2.0);

		prevLocalInscatterR = localInscatterR;
		prevLocalInscatterM = localInscatterM;
	}

	vec3 m = scatterM;

	ApplyPhaseFunction(scatterR, scatterM, dot(rayDir, -lightDir.xyz));
	vec3 lightInscatter = (scatterR * ScatteringR + scatterM * ScatteringM) * IncomingLight.xyz;
	lightInscatter += RenderSun(m, clampDot(rayDir, -lightDir.xyz)) * SunIntensity;
	vec3 lightExtinction = exp(-(densityCP.x * ExtinctionR + densityCP.y * ExtinctionM));

	extinction = vec4(lightExtinction, 0);

	return vec4(lightInscatter, 1.0);
}

void main()
{
	vec2 uv = v_texcoord;
	vec3 rayStart = CameraPos;

	vec3 wpos = v_position;

	vec3 rayDir = wpos - CameraPos;

	float rayLength = length(rayDir);
	rayDir /= rayLength;

	vec3 planetCenter = vec3(0, -PlanetRadius, 0);
	vec2 intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, PlanetRadius + AtmosphereHeight);

	rayLength = intersection.y;

	intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, PlanetRadius);
	if(intersection.x > 0)
	{
		rayLength = min(rayLength, intersection.x);
	}

	vec4 extinction;
	vec3 lightDir = normalize(LightDir.xyz);
	vec4 inscattering = IntegrateInscattering(rayStart, rayDir, rayLength, planetCenter, DistanceScale, lightDir, 16, extinction);

	gl_FragColor = inscattering;
}
