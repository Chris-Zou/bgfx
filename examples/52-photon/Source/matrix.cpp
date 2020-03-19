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
