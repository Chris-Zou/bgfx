#include "../Include/shape.h"
#include "bx/bx.h"
#include "../Include/matrix.h"
#include "../Include/ray.h"
#include <iostream>

Shape::Shape()
{
	m_material = &LAMBERTIAN;
}
Vector Shape::Reflect(const Vector& in, const Vector& normal)
{
	return in - normal * in.DotProduct(normal) * 2;
}

PhotonRay Shape::Refract(const PhotonRay& in, const Vector& point, const Vector& visibleNormal) const
{
	float n;
	if (visibleNormal == GetNormal(point))
		n = m_refracIndex;
	else
		n = 1.0f / m_refracIndex;

	float cosI = -visibleNormal.DotProduct(in.GetDirection());
	float sinT2 = n * n * (1 - cosI * cosI);
	if (sinT2 > 1)
	{
		return PhotonRay(point, Reflect(in.GetDirection(), visibleNormal));
	}

	float cosT = sqrt(1 - sinT2);
	Vector refracted = in.GetDirection() * n + visibleNormal * (n * cosI - cosT);

	return PhotonRay(point, refracted);
}

bool Shape::RussianRoulette(const ColoredRay& in, const Vector& point, ColoredRay& out) const
{
	float random = GetRandomValue();

	if (random < m_material->GetDiffuse(point).MeanRGB())
	{
		PoseTransformationMatrix localToWorld = PoseTransformationMatrix::GetPoseTransformation(point, GetVisibleNormal(point, in));

		float inclination, azimuth;
		std::tie(inclination, azimuth) = UniformCosineSampling();

		Vector localRay(sin(inclination) * cos(azimuth), cos(inclination) * cos(azimuth), sin(azimuth));

		out = ColoredRay(point, localToWorld * localRay, in.GetColor() * m_material->GetDiffuse(point) / (m_material->GetDiffuse(point).MeanRGB() + 0.00001f));

		return true;
	}
	else if (random < (m_material->GetDiffuse(point).MeanRGB() + m_material->GetSpecular().MeanRGB()))
	{
		PoseTransformationMatrix localToWorld = PoseTransformationMatrix::GetPoseTransformation(point, GetVisibleNormal(point, in));

		float inclination, azimuth;
		std::tie(inclination, azimuth) = PhongSpecularLobeSampling(m_material->GetShininess());

		Vector localRay(sin(inclination) * cos(azimuth), cos(inclination) * cos(azimuth), sin(azimuth));

		out = ColoredRay(point, localToWorld * localRay, in.GetColor() * m_material->GetSpecular() / (m_material->GetSpecular().MeanRGB() + 0.00001f) * (m_material->GetShininess() + 2) / (m_material->GetShininess() + 1));

		return true;
	}
	else if (random < (m_material->GetDiffuse(point).MeanRGB() + m_material->GetSpecular().MeanRGB() + m_material->GetReflectance().MeanRGB() + m_material->GetTransmittance().MeanRGB()))
	{
		PhotonRay refractedray = Refract(in, point, GetVisibleNormal(point, in));

		out = ColoredRay(point, refractedray.GetDirection(), in.GetColor() * m_material->GetTransmittance() / (m_material->GetTransmittance().MeanRGB() + 0.00001f));

		return true;
	}

	return false;
}

Vector Shape::GetVisibleNormal(const Vector& point, const PhotonRay& from) const
{
	return VisibleNormal(GetNormal(point), from.GetDirection());
}

Material* Shape::GetMaterial() const
{
	return m_material;
}

void Shape::SetMaterial(Material* mat)
{
	m_material = mat;
}

void Shape::SetRefractIndex(const float ri)
{
	m_refracIndex = ri;
}

Color Shape::GetEmittedLight()
{
	return m_emitted * m_powerEmitted;
}

void Shape::SetEmittedLight(const Color& emitted, const float power)
{
	m_emitted = emitted;
	m_powerEmitted = power;
	SetMaterial(&NONE);
}
