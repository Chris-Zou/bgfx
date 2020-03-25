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
	Vector v = ray.GetPosition() - m_center;
	float a = ray.GetDirection().DotProduct(ray.GetDirection());;
	float b = 2 * ray.GetDirection().DotProduct(v);
	float c = v.DotProduct(v) - m_radius2;

	float delta = b * b - 4 * a * c;

	if (delta < 0.0f)
	{
		return FLT_MAX;
	}
	else if (delta > 0.0f)
	{
		float t1 = (-b - sqrt(delta)) / (2 * a);
		float t2 = (-b + sqrt(delta)) / (2 * a);

		return GetNearestInFront(t1, t2);
	}
	else
	{
		float t = -b / (2 * a);
		return GetNearestInFront(t);
	}

	return 0.0f;
}

void PhotonSphere::Intersect(const PhotonRay& ray, float &min_t, Shape*& nearestShape, Shape*& thisShape) const
{
	float t = Intersect(ray);
	if (t < min_t)
	{
		min_t = t;
		nearestShape = thisShape;
	}
}

bool PhotonSphere::IsInside(const Vector& point) const
{
	return point.Distance(m_center) <= m_radius;
}

Vector PhotonSphere::GetNormal(const Vector& point) const
{
	return Vector(0.0f, 0.0f, 0.0f, 0.0f);
}
float PhotonSphere::Area(const float radius)
{
	return 2 * PI * radius * radius;
}
