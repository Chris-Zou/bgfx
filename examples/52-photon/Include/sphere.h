#pragma once

#include "shape.h"

class PhotonSphere : public Shape
{
public:
	PhotonSphere(const Vector& center, const float radius);
	float Intersect(const PhotonRay& ray) const;
	void Intersect(const PhotonRay& ray, float &min_t, Shape*& nearestShape, Shape*& thisShape) const;
	bool IsInside(const Vector& point) const;
	Vector GetNormal(const Vector& point) const;
	static float Area(const float radius);
private:
	Vector m_center;
	float m_radius;
	float m_radius2;
};
