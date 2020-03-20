#include "../Include/shape.h"

Shape::Shape()
{
	m_material = LAMBERTIAN;
}
Vector Shape::Reflect(const Vector& in, const Vector& normal)
{
	return in - normal * in.DotProduct(normal) * 2;
}

Ray Shape::Refract(const Ray& in, const Vector& point, const Vector& visibleNormal) const
{
	float n;
	if (visibleNormal == GetNormal(point))
		n = m_refracIndex;
	else
		n = 1.0 / m_refracIndex;

	float cosI = -visibleNormal.DotProduct(in.GetDirection());
	float sinT2 = n * n * (1 - cosI * cosI);
	if (sinT2 > 1)
	{
		return Ray(point, Reflect(in.GetDirection(), visibleNormal));
	}

	float cosT = sqrt(1 - sinT2);
	Vector refracted = in.GetDirection() * n + visibleNormal * (n * cosI - cosI);

	return Ray(point, refracted);
}

bool Shape::RussianRoulette(const ColoredRay& in, const Vector& point, ColoredRay& out) const
{
	return true;
}

Vector Shape::GetVisibleNormal(const Vector& point, const Ray& from) const
{
	return VisibleNormal(GetNormal(point), from.GetDirection());
}

Material* Shape::GetMaterial() const
{
	return m_material;
}

void Shape::SetMaterial(Material* mat)
{
	m_material = mat;
}

void Shape::SetRefractIndex(const float ri)
{
	m_refracIndex = ri;
}

Color Shape::GetEmittedLight()
{
	return m_emitted * m_powerEmitted;
}

void Shape::SetEmittedLight(const Color& emitted, const float power)
{
	m_emitted = emitted;
	m_powerEmitted = power;
	SetMaterial(NONE);
}
