#pragma once

#include "vector.h"

class Matrix
{
public:
	Matrix(const float* value);
	Vector operator*(const Vector& p) const;
	Matrix operator*(const Matrix& m) const;
	bool operator==(const Matrix& m) const;
	bool operator!=(const Matrix& m) const;
	friend std::ostream& operator<<(std::ostream& out, const Matrix& mat);
private:
	float m_data[16];
};
