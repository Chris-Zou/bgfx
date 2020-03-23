#pragma once

#include "lightSource.h"
#include "vector.h"


class PointLight : public LightSource
{
public:
	PointLight();
	PointLight(const Vector& pos);
	PointLight(const Vector& pos, const float power, const Color& baseColor);
	Color GetColor(const Vector& point) const;
	std::vector<Vector> GetLights() const;

private:
	Vector m_position;
};
