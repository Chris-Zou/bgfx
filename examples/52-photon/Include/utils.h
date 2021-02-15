#pragma once

#include <random>
#include <tuple>
#include "vector.h"

static constexpr float PI = 3.1415926535897932f;

const float INTERSECTION_TH = 0.00001f;

static constexpr float VACUUM_RI = 1.0f;
static constexpr float AIR_RI = 1.0002926f;
static constexpr float WATER_RI = 1.333f;
static constexpr float QUARTZ_RI = 1.544f;
static constexpr float GLASS_RI = 1.52f;
static constexpr float DIAMOND_RI = 2.42f;

inline static float GetRandomValue()
{
	static std::random_device randDev;
	static std::mt19937 mt(randDev());
	static std::uniform_real_distribution<float> distribution(0, 1);

	return distribution(mt);
}

inline static std::tuple<float, float> UniformCosineSampling()
{
	float inclination = acos(sqrt(1 - GetRandomValue()));
	float azimuth = 2 * PI * GetRandomValue();

	return std::make_tuple(inclination, azimuth);
}

inline static std::tuple<float, float> UniformSphereSampling()
{
	float inclination = acos(2 * GetRandomValue() - 1.0f);
	float azimuth = 2 * PI * GetRandomValue();

	return std::make_tuple(inclination, azimuth);
}

inline static std::tuple<float, float> PhongSpecularLobeSampling(const float alpha)
{
	float inclination = acos(pow(GetRandomValue(), 1.0f / (alpha + 1.0f)));
	float azimuth = 2 * PI * GetRandomValue();

	return std::make_tuple(inclination, azimuth);
}

inline static Vector VisibleNormal(const Vector& normal, const Vector& from)
{
	float cosine = normal.DotProduct(from);
	if ((cosine > 0) | ((cosine == 0) & (normal == from)))
		return normal * -1;
	else
		return normal;
}

inline static float GetNearestInFront(const float t)
{
	return (t > INTERSECTION_TH) ? t : FLT_MAX;
}

inline static float GetNearestInFront(const float t_1, const float t_2)
{
	if ((t_1 > INTERSECTION_TH) & ((t_1 < t_2) | (t_2 <= INTERSECTION_TH)))
	{
		return t_1;
	}
	else if ((t_2 > INTERSECTION_TH) & ((t_2 < t_1) | (t_1 <= INTERSECTION_TH)))
	{
		return t_2;
	}
	else
	{
		return FLT_MAX;
	}
}