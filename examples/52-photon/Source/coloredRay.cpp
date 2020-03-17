#include "../Include/coloredRay.h"

ColoredRay::ColoredRay() {}

ColoredRay::ColoredRay(const Vector& pos, const Vector& dir, const Color& c)
	: Ray(pos, dir)
	, m_color(c)
{}

Color ColoredRay::GetColor() const
{
	return m_color;
}
