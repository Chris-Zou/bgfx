#include "../Include/plane.h"

Plane::Plane(const Vector& point, const Vector& normal)
{

}

float Plane::Intersect(const Ray& ray) const
{

}

void Plane::Intersect(const Ray& ray, float& min_t, Shape*& nearestShape, Shape* thisShape) const
{

}

bool Plane::IsInside(const Vector& point) const
{

}

Vector Plane::GetNormal() const
{
	return m_normal;
}

Vector Plane::GetNormal(const Vector& point) const
{
	return m_normal;
}
