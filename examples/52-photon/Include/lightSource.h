#pragma once

#include "color.h"
#include "ray.h"
#include "vector.h"
#include <vector>
#include <memory>

class LightSource
{
public:
	virtual Color GetColor(const Vector& point) const = 0;
	virtual std::vector<Vector> GetLights() const = 0;
	Color GetBaseColor()
	{
		return m_baseColor * m_power;
	}
protected:
	LightSource()
		: m_power(2.0f)
		, m_baseColor(WHITE)
	{}

	LightSource(const float power, const Color& c)
		: m_power(power)
		, m_baseColor(c)
	{}

protected:
	float m_power;
	Color m_baseColor;
};
