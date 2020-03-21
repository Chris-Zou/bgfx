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
	virtual vector<Vector> GetLights() const = 0;
	Color GetBaseColor()
	{
		return m_baseColor * m_power;
	}
protected:
	LightSource();
	LightSource(const float power, const Color& c);
private:
	float m_power;
	Color m_baseColor;
};
