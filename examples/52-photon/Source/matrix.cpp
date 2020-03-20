#include "../Include/matrix.h"

Matrix::Matrix(const float* values)
{
	if (values)
	{
		for (int i = 0; i < 16; ++i)
		{
			m_data[i] = values[i];
		}
	}
}

Vector Matrix::operator*(const Vector& p) const
{
	float x = Vector(m_data[0], m_data[1], m_data[2], m_data[3]).DotProduct(p);
	float y = Vector(m_data[4], m_data[5], m_data[6], m_data[7]).DotProduct(p);
	float z = Vector(m_data[8], m_data[9], m_data[10], m_data[11]).DotProduct(p);
	float w = Vector(m_data[12], m_data[13], m_data[14], m_data[15]).DotProduct(p);
}

Matrix Matrix::operator*(const Matrix& mat) const
{

}

bool Matrix::operator==(const Matrix& m) const
{

}

bool Matrix::operator!=(const Matrix& m) const
{
	return !(*this == m);
}

std::ostream& operator<<(std::ostream& out, const Matrix &m)
{

}
