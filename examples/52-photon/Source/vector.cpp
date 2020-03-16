#include "../Include/vector.h"
#include <cmath>

Vector::Vector() {}

Vector::Vector(const float _x, const float _y, const float _z)
	:m_x(_x)
	, m_y(_y)
	, m_z(_z)
{}

float Vector::GetX() const
{
	return m_x;
}

float Vector::GetY() const
{
	return m_y;
}

float Vector::GetZ() const
{
	return m_z;
}

float Vector::Length() const
{
	return sqrtf(m_x * m_x + m_y * m_y + m_z * m_z);
}

Vector Vector::Normalize() const
{
	return *this / Length();
}

Vector Vector::operator*(const float s) const
{
	return Vector(m_x * s, m_y * s, m_z * s);
}

Vector Vector::operator/(const float s) const
{
	if(s < H)
}
