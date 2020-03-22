#include "../Include/vector.h"
#include <cmath>

Vector::Vector() {}

Vector::Vector(const float _x, const float _y, const float _z)
	: m_x(_x)
	, m_y(_y)
	, m_z(_z)
{}

Vector::Vector(const float _x, const float _y, const float _z, const float _w)
	: m_x(_x)
	, m_y(_y)
	, m_z(_z)
	, m_w(_w)
{

}

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

float Vector::GetW() const
{
	return m_w;
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
	if (abs(s) < TH)
		return Vector(FLT_MAX, FLT_MAX, FLT_MAX);

	return Vector(m_x / s, m_y / s, m_z / s);
}

Vector Vector::operator+(const Vector& v) const
{
	return Vector(m_x + v.m_x, m_y + v.m_y, m_z + v.m_z);
}

Vector Vector::operator-(const Vector& v) const
{
	return Vector(m_x - v.m_x, m_y - v.m_y, m_z - v.m_z);
}

void Vector::operator+=(const Vector& v)
{
	*this = *this + v;
}

void Vector::operator-=(const Vector& v)
{
	*this = *this - v;
}


float Vector::DotProduct(const Vector& v) const
{
	return m_x * v.m_x + m_y * v.m_y + m_z * v.m_z;
}

Vector Vector::CrossProduct(const Vector& v) const
{
	float x = m_y * v.m_z - m_z * v.m_y;
	float y = m_z * v.m_x - m_x * v.m_z;
	float z = m_x * v.m_y - m_y * v.m_x;

	return Vector(x, y, z);
}

bool Vector::operator==(const Vector& v) const
{
	return (m_x == v.m_x && m_y == v.m_y && m_z == v.m_z);
}

bool Vector::operator!=(const Vector& v) const
{
	return !(*this == v);
}

float Vector::Distance(const Vector& p) const
{
	return (*this - p).Length();
}

float Vector::operator[](const unsigned int index) const
{
	switch (index)
	{
	case 0:
		return m_x;
	case 1:
		return m_y;
	case 2:
		return m_z;
	default:
		return 0.0f;
	}
}

std::ostream& operator<<(std::ostream& out, const Vector& v)
{
	out << "Vector(" << v.m_x << ", " << v.m_y << ", " << v.m_z << ")";

	return out;
}
