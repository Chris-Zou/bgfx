#pragma once

#include <bgfx/bgfx.h>
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <vector>
#include <unordered_map>


namespace Dolphin
{
	struct Mesh;

	extern std::vector<glm::vec3> basicLightVolumeShapePositions;

	class lightVolumeShape
	{
	public:
		lightVolumeShape(uint8_t detail);

		Mesh getMesh();

		std::vector<glm::vec3> vertices;
		std::vector<uint16_t> indices;

	private:
		uint8_t detail;
		std::unordered_map<uint32_t, uint16_t> newVertices;

		uint16_t getOrCreateMidPoint(uint16_t first, uint16_t second);
	};
}

