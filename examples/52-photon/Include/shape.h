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
	virtual float Intersect(const Ray& lightRay) const = 0;
	virtual void Intersect(const Ray& lightRay, float& minT, Shape*& nearestShape, Shape*& thisShape) const = 0;
	static Vector Reflect(const Vector& in, const Vector& normal);
	Ray Refract(const LightRay& in, const Vector& point, const Vector& visibleNormal) const;
	bool RussianRoulette(const ColoredLightRay &in, const Point &point, ColoredLightRay &out, bool &isCaustic) const;
	virtual bool IsInside(const Vector& point) const;
	virtual Vector GetNormal(const Vector& point) const = 0;
	Vector GetVisibleNormal(const Vector& point, const Ray& from) const;
	Material* GetMaterial() const;
	virtual void SetMaterial(const Material* mat);
	virtual void SetRefractIndex(const float ri);
	Color GetEmittedLight();
	void SetEmittedLight(const Color& emitted, const float power);
private:
	Material* m_material = LAMBERTIAN;
	float m_refracIndex = AIR_RI;
	Color m_emitted = BLACK;
	float m_powerEmitted = 0.0f;
};
