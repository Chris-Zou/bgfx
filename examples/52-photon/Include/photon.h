#pragma once

#include "coloredRay.h"
#include "vector.h"

class Photon
{
public:
	Photon();
	Photon(const Color& flux, const Vector& incidence);
	Photon(const ColoredRay& lightRay);
	Vector GetIncidence();
	Color GetFlux();
private:
	Color m_flux;
	Vector m_incidence;
};
