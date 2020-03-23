#pragma once

#include <cmath>
#include "coloredRay.h"
#include "ray.h"
#include "material.h"
#include "utils.h"
#include <memory>

class Shape
{
public:
	Shape();
	virtual float Intersect(const PhotonRay& lightRay) const = 0;
	virtual void Intersect(const PhotonRay& lightRay, float& minT, Shape*& nearestShape, Shape*& thisShape) const = 0;
	static Vector Reflect(const Vector& in, const Vector& normal);
	PhotonRay Refract(const PhotonRay& in, const Vector& point, const Vector& visibleNormal) const;
	bool RussianRoulette(const ColoredRay &in, const Vector &point, ColoredRay &out) const;
	//virtual bool IsInside(const Vector& point) const;
	virtual Vector GetNormal(const Vector& point) const = 0;
	Vector GetVisibleNormal(const Vector& point, const PhotonRay& from) const;
	Material* GetMaterial() const;
	virtual void SetMaterial(Material* mat);
	virtual void SetRefractIndex(const float ri);
	Color GetEmittedLight();
	void SetEmittedLight(const Color& emitted, const float power);
private:
	Material* m_material;
	float m_refracIndex = AIR_RI;
	Color m_emitted = BLACK;
	float m_powerEmitted = 0.0f;
};
