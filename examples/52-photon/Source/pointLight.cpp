
#include "../Include/pointLight.h"

PointLight::PointLight()
	: LightSource()
	, m_position(Vector(0.0f, 0.0f, 0.0f))
{}

PointLight::PointLight(const Vector& pos)
	: LightSource()
	, m_position(pos)
{}

PointLight::PointLight(const Vector& pos, const float power, const Color& baseColor)
	: LightSource(power, baseColor)
	, m_position(pos)
{}

Color PointLight::GetColor(const Vector& point) const
{
	float distance = point.Distance(m_position);

	return m_baseColor * (m_power / distance * distance);
}

std::vector<Vector> PointLight::GetLights() const
{
	return std::vector<Vector>{m_position};
}
