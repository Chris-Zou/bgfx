#include "../Include/sphere.h"

PhotonSphere::PhotonSphere(const Vector& center, const float radius)
	: Shape()
	, m_center(center)
	, m_radius(radius)
	, m_radius2(m_radius * m_radius)
{

}

float PhotonSphere::Intersect(const PhotonRay& ray) const
{
	return 0.0f;
}

void PhotonSphere::Intersect(const PhotonRay& ray, float &min_t, Shape*& nearestShape, Shape*& thisShape) const
{

}


bool PhotonSphere::IsInside(const Vector& point) const
{
	return false;
}

Vector PhotonSphere::GetNormal(const Vector& point) const
{
	return Vector(0.0f, 0.0f, 0.0f, 0.0f);
}
float PhotonSphere::Area(const float radius)
{
	return 2 * PI * radius * radius;
}
