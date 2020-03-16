#pragma once

#include "vector.h"

class Ray
{
public:
	Ray();
	Ray(const Vector& pos, const Vector& dir);
	std::tuple<float, float> Distance(const Vector& to) const;
	Vector GetScaledPosition(const float s) const;
	Vector GetPosition() const;
	Vector GetDirection() const;

private:
	Vector m_pos;
	Vector m_dir;
};
