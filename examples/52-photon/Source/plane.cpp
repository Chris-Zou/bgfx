#include "../Include/plane.h"
#include "bx/bx.h"

Plane::Plane(const Vector& point, const Vector& normal)
	: Shape()
	, m_point(point)
	, m_normal(normal)
{

}

float Plane::Intersect(const PhotonRay& ray) const
{
	float denominator = ray.GetDirection().DotProduct(m_normal);
	float numerator = (m_point - ray.GetPosition()).DotProduct(m_normal);

	if (denominator != 0)
	{
		float t = numerator / denominator;

		if (t > 0.00001f)
			return t;
		else
			return FLT_MAX;
	}
	else if (numerator != 0)
	{
		return FLT_MAX;
	}
	else
	{
		return 0.0001f;
	}
}

void Plane::Intersect(const PhotonRay& ray, float& min_t, Shape*& nearestShape, Shape*& thisShape) const
{
	float t = Intersect(ray);
	if (t < min_t)
	{
		min_t = t;
		nearestShape = thisShape;
	}
}

bool Plane::IsInside(const Vector& point) const
{
	BX_UNUSED(point);
	throw 1;
}

Vector Plane::GetNormal() const
{
	return m_normal;
}

Vector Plane::GetNormal(const Vector& point) const
{
	BX_UNUSED(point);
	return m_normal;
}
