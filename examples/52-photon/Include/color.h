#pragma once

#include <algorithm>
#include <climits>
#include <cmath>

class Color
{
public:
	constexpr Color()
		: m_r(0.0f)
		, m_g(0.0f)
		, m_b(0.0f)
	{}

	constexpr Color(const float _r, const float _g, const float _b)
		: m_r(_r)
		, m_g(_g)
		, m_b(_b)
	{}

	constexpr float GetR() const
	{
		return m_r;
	}

	constexpr float GetG() const
	{
		return m_g;
	}

	constexpr float GetB() const
	{
		return m_b;
	}

	Color operator+(const Color& _c)
	{
		return Color(m_r + _c.m_r, m_g + _c.m_g, m_b + _c.m_b)
	}

	void operator+=(const Color& _c)
	{
		m_r += _c.m_r;
		m_g += _c.m_g;
		m_b += _c.m_b;
	}

	Color operator*(const Color& _c) const
	{
		return Color(m_r * _c.m_r, m_g * _c.m_g, m_b * _c.m_b);
	}

	Color operator*(const float s) const
	{
		return Color(m_r * s, m_g * s, m_b * s);
	}

	void operator*=(const float s)
	{
		*this = *this * s;
	}

	Color operator/(const float s) const
	{
		return Color(m_r / s, m_g / s, m_b / s);
	}

	bool operator==(const Color& _c) const
	{
		return (m_r == _c.m_r && m_g == _c.m_g && m_b == _c.m_b);
	}

	bool operator!=(const Color& _c) const
	{
		return !(*this == _c);
	}

	Color Clamp() const
	{
		return Color(std::max(0.0f, std::min(m_r, 1.0f)), std::max(0.0f, std::min(m_g, 1.0f)), std::max(0.0f, std::min(m_b, 1.0f)));
	}

	Color GammaCorrect() const
	{
		return Color(pow(m_r, GAMMA), pow(m_g, GAMMA), pow(m_r, GAMMA));
	}

	float MeanRGB() const
	{
		return (m_r + m_g + m_b) / 3.0f;
	}

private:
	static constexpr float GAMMA = 2.2f;

	float m_r;
	float m_g;
	float m_b;
};

static constexpr Color WHITE(0.85f, 0.85f, 0.85f);
static constexpr Color GRAY(0.35f, 0.35f, 0.35f);
static constexpr Color BLACK(0.0f, 0.0f, 0.0f);
static constexpr Color RED(0.85f, 0.0f, 0.0f);
static constexpr Color GREEN(0.0f, 0.85f, 0.0f);
static constexpr Color BLUE(0.0f, 0.0f, 0.85f);
static constexpr Color YELLOW(0.85f, 0.85f, 0.0f);
static constexpr Color PURPLE(0.85f, 0.0f, 0.85f);
static constexpr Color SKY_BLUE(0.0f, 0.85f, 0.85f);
