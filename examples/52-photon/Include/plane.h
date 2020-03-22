#pragma once

#include "shape.h"

class Plane : public Shape
{
public:
	Plane(const Vector& point, const Vector& normal);
	float Intersect(const Ray& ray) const;
	void Intersect(const Ray& ray, float& min_t, Shape*& nearestShape, Shape*& thisShape) const;
	bool IsInside(const Vector& point) const;
	Vector GetNormal() const;
	virtual Vector GetNormal(const Vector& point) const;

private:
	Vector m_point;
	Vector m_normal;
};
