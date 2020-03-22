#include "../Include/sphere.h"

Sphere::Sphere(const Vector& center, const float radius)
	: Shape()
	, m_center(center)
	, m_radius(radius)
	, m_radius2(m_radius * m_radius)
{

}

float Sphere::Intersect(const Ray& ray) const
{
	return 0.0f;
}

void Sphere::Intersect(const Ray& ray, float &min_t, Shape* nearestShape, Shape* thisShape) const
{

}


bool Sphere::IsInside(const Vector& point) const
{
	return false;
}

Vector Sphere::GetNormal(const Vector& point) const
{
	return Vector(0.0f, 0.0f, 0.0f, 0.0f);
}
float Sphere::Area(const float radius)
{
	return 2 * PI * radius * radius;
}
