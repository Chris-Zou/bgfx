#pragma once

#include <string>

namespace Dolphin
{
	struct Model;
	Model loadGltfModel(const std::string& assetPath, const std::string& fileName);
}
