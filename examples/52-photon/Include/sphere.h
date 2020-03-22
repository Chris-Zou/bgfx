#pragma once

#include "shape.h"

class Sphere : public Shape
{
public:
	Sphere(const Vector& center, const float radius);
	float Intersect(const Ray& ray) const;
	void Intersect(const Ray& ray, float &min_t, Shape* nearestShape, Shape* thisShape) const;
	bool IsInside(const Vector& point) const;
	Vector GetNormal(const Vector& point) const;
	static float Area(const float radius);
private:
	Vector m_center;
	float m_radius;
	float m_radius2;
};
