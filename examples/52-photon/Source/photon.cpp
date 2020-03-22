#include "../Include/photon.h"

Photon::Photon() {}

Photon::Photon(const Color& flux, const Vector& incidence)
	: m_flux(flux)
	, m_incidence(incidence)
{}

Photon::Photon(const ColoredRay& lightRay)
	: Photon(lightRay.GetColor(), lightRay.GetDirection())
{
}

Vector Photon::GetIncidence()
{
	return m_incidence;
}

Color Photon::GetFlux()
{
	return m_flux;
}
