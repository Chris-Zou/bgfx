#pragma once

#include "color.h"
#include "ray.h"

class ColoredRay : public Ray
{
public:
	ColoredRay();
	ColoredRay(const Vector& pos, const Vector& dir, const Color& c);
	Color GetColor() const;
private:
	Color m_color;
};
