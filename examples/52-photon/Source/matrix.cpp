#include "../Include/matrix.h"
#include "bx/bx.h"

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

	return Vector(x, y, z, w);
}

Matrix Matrix::operator*(const Matrix& mat) const
{
	BX_UNUSED(mat);

	return Matrix(nullptr);
}

bool Matrix::operator==(const Matrix& m) const
{
	BX_UNUSED(m);
	return true;
}

bool Matrix::operator!=(const Matrix& m) const
{
	return !(*this == m);
}

std::ostream& operator<<(std::ostream& out, const Matrix &m)
{
	BX_UNUSED(m);

	return out;
}

PoseTransformationMatrix::PoseTransformationMatrix(const Vector& origin, const Vector& xAxis, const Vector& yAxis, const Vector& zAxis)
	: Matrix(nullptr)
{
	m_data[0] = xAxis.GetX();	m_data[1] = yAxis.GetX();	m_data[2] = zAxis.GetX();	m_data[3] = origin.GetX();
	m_data[4] = xAxis.GetY();	m_data[5] = yAxis.GetY();	m_data[6] = zAxis.GetY();	m_data[7] = origin.GetY();
	m_data[8] = xAxis.GetZ();	m_data[9] = yAxis.GetZ();	m_data[10] = zAxis.GetZ();	m_data[11] = origin.GetZ();
	m_data[12] = 0;				m_data[13] = 0;				m_data[14] = 0;				m_data[15] = 1;
}

PoseTransformationMatrix PoseTransformationMatrix::GetPoseTransformation(const Vector& point, const Vector& zAxis)
{
	Vector xAxis;

	if (zAxis.GetX() != 0)
	{
		xAxis = zAxis.CrossProduct(Vector(0, 0, 1)).Normalize();
	}
	else
	{
		xAxis = zAxis.CrossProduct(Vector(1, 0, 0)).Normalize();
	}

	Vector yAxis = xAxis.CrossProduct(zAxis);
	return PoseTransformationMatrix(point, xAxis, yAxis, zAxis);
}

PoseTransformationMatrix PoseTransformationMatrix::Inverse() const
{
	Vector x(m_data[0], m_data[1], m_data[2]);
	Vector y(m_data[4], m_data[5], m_data[6]);
	Vector z(m_data[8], m_data[9], m_data[10]);

	float cX = m_data[0] * m_data[3] + m_data[4] * m_data[7] + m_data[8] * m_data[11];
	float cY = m_data[1] * m_data[3] + m_data[5] * m_data[7] + m_data[9] * m_data[11];;
	float cZ = m_data[2] * m_data[3] + m_data[6] * m_data[7] + m_data[10] * m_data[11];;
	Vector c(-cX, -cY, -cZ);

	return PoseTransformationMatrix(c, x, y, z);
}
