#include "../Include/plane.h"

Plane::Plane(const Vector& point, const Vector& normal)
	: Shape()
	, m_point(point)
	, m_normal(normal)
{

}

float Plane::Intersect(const PhotonRay& ray) const
{
	return 0.0f;
}

void Plane::Intersect(const PhotonRay& ray, float& min_t, Shape*& nearestShape, Shape*& thisShape) const
{

}

bool Plane::IsInside(const Vector& point) const
{
	return false;
}

Vector Plane::GetNormal() const
{
	return m_normal;
}

Vector Plane::GetNormal(const Vector& point) const
{
	return m_normal;
}
