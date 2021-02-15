#pragma once

#include <array>

enum EDimension
{
	X,
	Y,
	Z,
	NO_DIM
};

class Vector
{
public:
	static constexpr float TH = 0.0000001f;

	Vector();
	Vector(const float _x, const float _y, const float _z);
	Vector(const float _x, const float _y, const float _z, const float _w);
	float GetX() const;
	float GetY() const;
	float GetZ() const;
	float GetW() const;

	float Length() const;
	Vector Normalize() const;
	float Distance(const Vector& p) const;
	Vector operator*(const float s) const;
	Vector operator/(const float s) const;
	Vector operator+(const Vector& v) const;
	Vector operator-(const Vector& v) const;
	void operator+=(const Vector& v);
	void operator-=(const Vector& v);
	float DotProduct(const Vector& v) const;
	Vector CrossProduct(const Vector& v) const;
	bool operator==(const Vector& v) const;
	bool operator!=(const Vector& v) const;
	float operator[](const unsigned int index) const;
	
	friend std::ostream& operator<<(std::ostream& out, const Vector& v);

private:
	float m_x;
	float m_y;
	float m_z;
	float m_w;
};