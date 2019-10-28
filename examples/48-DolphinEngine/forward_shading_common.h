#ifndef __FORWARDSHADING_COMMON_H__
#define __FORWARDSHADING_COMMON_H__

#include <vector>
#include <glm/gtc/type_ptr.hpp>
#include <bx/readerwriter.h>

#include "mtlloader.h"
#include "bgfx_utils.h"
#include "entry/entry.h"
#include "camera.h"

#include "shader_defines.sh"


uint32_t packUint32(uint8_t _x, uint8_t _y, uint8_t _z, uint8_t _w);
uint32_t packF4u(float _x, float _y = 0.0f, float _z = 0.0f, float _w = 0.0f);

struct PosNormalTexcoordVertex
{
	float m_x;
	float m_y;
	float m_z;
	uint32_t m_normal;
	float m_u;
	float m_v;
};

struct PosNormalTangentTexcoordVertex
{
	float m_x;
	float m_y;
	float m_z;
	uint32_t m_normal;
	uint32_t m_tangent;
	float m_u;
	float m_v;
};

struct Light
{
	union Position
	{
		struct
		{
			float m_x;
			float m_y;
			float m_z;
			float m_w;
		};

		float m_v[4];
	};

	Position m_position;
};

struct Uniforms
{
	void init()
	{
		u_params0 = bgfx::createUniform("u_params0", bgfx::UniformType::Vec4);
		u_params1 = bgfx::createUniform("u_params1", bgfx::UniformType::Vec4);
		u_quadPoints = bgfx::createUniform("u_quadPoints", bgfx::UniformType::Vec4, 4);
		u_samples = bgfx::createUniform("u_samples", bgfx::UniformType::Vec4, NUM_SAMPLES);

		u_albedo = bgfx::createUniform("u_albedo", bgfx::UniformType::Vec4);
		u_color = bgfx::createUniform("u_color", bgfx::UniformType::Vec4);

		u_lightMtx = bgfx::createUniform("u_lightMtx", bgfx::UniformType::Mat4);

		u_lightPosition = bgfx::createUniform("u_lightPosition", bgfx::UniformType::Vec4);
		u_viewPosition = bgfx::createUniform("u_viewPosition", bgfx::UniformType::Vec4);
	}

	void setPtrs(Light* _lightPtr, float* _lightMtxPtr)
	{
		m_lightMtxPtr = _lightMtxPtr;
		m_lightPtr = _lightPtr;
	}

	// Call this once per frame.
	void submitPerFrameUniforms(glm::vec4 viewPos)
	{
		bgfx::setUniform(u_params0, m_params0);
		bgfx::setUniform(u_samples, m_samples, NUM_SAMPLES);
		bgfx::setUniform(u_lightPosition, &m_lightPtr->m_position);
		bgfx::setUniform(u_viewPosition, &viewPos[0]);

		bgfx::setUniform(u_albedo, &m_albedo[0]);
		bgfx::setUniform(u_color, &m_color[0]);
	}

	void submitPerLightUniforms()
	{
		bgfx::setUniform(u_params1, m_params1);

		bgfx::setUniform(u_quadPoints, m_quadPoints, 4);
	}

	// Call this before each draw call.
	void submitPerDrawUniforms()
	{
		//bgfx::setUniform(u_lightMtx, m_lightMtxPtr);
	}

	void destroy()
	{
		bgfx::destroy(u_params0);
		bgfx::destroy(u_params1);
		bgfx::destroy(u_quadPoints);
		bgfx::destroy(u_samples);

		bgfx::destroy(u_albedo);
		bgfx::destroy(u_color);

		bgfx::destroy(u_lightMtx);
		bgfx::destroy(u_lightPosition);
		bgfx::destroy(u_viewPosition);
	}

	union
	{
		struct
		{
			float m_reflectance;
			float m_roughness;
			float m_unused02;
			float m_sampleCount;
		};

		float m_params0[4];
	};

	union
	{
		struct
		{
			float m_lightIntensity;
			float m_twoSided;
			float m_unused22;
			float m_unused23;
		};

		float m_params1[4];
	};

	glm::vec4 m_samples[NUM_SAMPLES];
	glm::vec4 m_quadPoints[4];

	glm::vec4 m_albedo;
	glm::vec4 m_color;

	float* m_lightMtxPtr;
	float* m_colorPtr;
	Light* m_lightPtr;

private:
	bgfx::UniformHandle u_params0;
	bgfx::UniformHandle u_params1;
	bgfx::UniformHandle u_quadPoints;
	bgfx::UniformHandle u_samples;
	bgfx::UniformHandle u_albedo;
	bgfx::UniformHandle u_color;

	bgfx::UniformHandle u_lightMtx;
	bgfx::UniformHandle u_lightPosition;
	bgfx::UniformHandle u_viewPosition;

};

struct RenderState
{
	enum Enum
	{
		Default = 0,
		ZPass,
		ZTwoSizePass,
		ColorPass,
		ColorAlphaPass,
		Count
	};

	uint64_t m_state;
	uint32_t m_blendFactorRgba;
	uint32_t m_fstencil;
	uint32_t m_bstencil;
};

struct ViewState
{
	ViewState(uint32_t _width = 1280, uint32_t _height = 720)
		: m_width(_width)
		, m_height(_height)
		, m_oldWidth(0)
		, m_oldHeight(0)
	{
		memset(m_oldView, 16 * sizeof(float), 0);
	}

	uint32_t m_width;
	uint32_t m_height;

	uint32_t m_oldWidth;
	uint32_t m_oldHeight;
	uint32_t m_oldReset;

	float m_view[16];
	float m_proj[16];

	float m_oldView[16];
};

struct ClearValues
{
	ClearValues(uint32_t _clearRgba = 0x30303000, float _clearDepth = 1.0f, uint8_t _clearStencil = 0)
		:m_clearRgba(_clearRgba), m_clearDepth(_clearDepth), m_clearStencil(_clearStencil)
	{

	}
	uint32_t	m_clearRgba;
	float		m_clearDepth;
	uint8_t		m_clearStencil;
};

namespace Dolphin
{
	struct Aabb
	{
		float m_min[3];
		float m_max[3];
	};

	struct Obb
	{
		float m_mtx[16];
	};

	struct Sphere
	{
		float m_center[3];
		float m_radius;
	};

	struct Primitive
	{
		uint32_t m_startIndex;
		uint32_t m_numIndices;
		uint32_t m_startVertex;
		uint32_t m_numVertices;

		Sphere m_sphere;
		Aabb m_aabb;
		Obb m_obb;
	};

	typedef std::vector<Primitive> PrimitiveArray;

	struct Group
	{
		Group()
		{
			reset();
		}

		void reset()
		{
			m_vbh.idx = BGFX_INVALID_HANDLE;
			m_ibh.idx = BGFX_INVALID_HANDLE;
			m_prims.clear();
		}

		bgfx::VertexBufferHandle m_vbh;
		bgfx::IndexBufferHandle m_ibh;
		Sphere m_sphere;
		Aabb m_aabb;
		Obb m_obb;

		struct Material
		{
			bgfx::TextureHandle m_metallicMap;
			bgfx::TextureHandle m_diffuseMap;
			bgfx::TextureHandle m_nmlMap;
			bgfx::TextureHandle m_roughnessMap;

			glm::vec3 m_diffuseTint;
			glm::vec3 m_specTint;
			float m_roughness;
		} m_material;

		PrimitiveArray m_prims;
	};
}

namespace bgfx
{
	int32_t read(bx::ReaderI* _reader, bgfx::VertexLayout& _decl, bx::Error* _err = NULL);
}

struct LightMaps
{
	LightMaps() {}

	void destroyTextures()
	{
		bgfx::destroy(m_colorMap);
		bgfx::destroy(m_filteredMap);
	}

	bgfx::TextureHandle m_colorMap;
	bgfx::TextureHandle m_filteredMap;
};

struct GlobalRenderingData
{
	Uniforms m_uniforms;

	bgfx::UniformHandle m_uColorMap;
	bgfx::UniformHandle m_uFilteredMap;

	LightMaps m_texStainedGlassMaps;
	LightMaps m_texWhiteMaps;
};

namespace Dolphin
{
	struct Mesh
	{
		bgfx::TextureHandle loadTexturePriv(const std::string& _filename, std::string _fallback = "", uint32_t _sampleFlags = 0)
		{
			if (_fallback == "")
				_fallback = _filename;

			std::string fileName = _filename == "" ? _fallback : _filename;

			bgfx::TextureHandle th;
			if (m_textureCache.find(fileName) != m_textureCache.end())
			{
				th = m_textureCache[fileName];
			}
			else
			{
				th = loadTexture(fileName.c_str(), _sampleFlags);
				m_textureCache[fileName] = th;
			}

			return th;
		}

		void load(const void* _vertices, uint32_t _numVertices, const bgfx::VertexLayout _layout, const uint16_t* _indices, uint32_t _numIndices)
		{
			Group group;
			const bgfx::Memory* mem;
			uint32_t size;

			size = _numVertices * _layout.getStride();
			mem = bgfx::makeRef(_vertices, size);
			group.m_vbh = bgfx::createVertexBuffer(mem, _layout);

			size = _numIndices * 2;
			mem = bgfx::makeRef(_indices, size);
			group.m_ibh = bgfx::createIndexBuffer(mem);

			group.m_material.m_metallicMap = loadTexturePriv("black.png");
			group.m_material.m_diffuseMap = loadTexturePriv("white.png");
			group.m_material.m_nmlMap = loadTexturePriv("nml.tga");
			group.m_material.m_roughnessMap = loadTexturePriv("white.png");

			m_groups.push_back(group);
		}

		void load(const char* _fileName)
		{
#define BGFX_CHUNK_MAGIC_VB  BX_MAKEFOURCC('V', 'B', ' ', 0x1)
#define BGFX_CHUNK_MAGIC_IB  BX_MAKEFOURCC('I', 'B', ' ', 0x0)
#define BGFX_CHUNK_MAGIC_PRI BX_MAKEFOURCC('P', 'R', 'I', 0x0)

			// load material file
			std::string fileStr = _fileName;
			std::string mtlFilePath = fileStr.substr(0, fileStr.find_last_of(".")) + ".mtl";
			std::map<std::string, MaterialDef> mtlDefs = LoadMaterialFile(mtlFilePath);

			bx::FileReaderI* reader = entry::getFileReader();
			bx::open(reader, _fileName);

			Group group;

			uint32_t chunk;
			while (4 == bx::read(reader, chunk))
			{
				switch (chunk)
				{
				case BGFX_CHUNK_MAGIC_VB:
				{
					bx::read(reader, group.m_sphere);
					bx::read(reader, group.m_aabb);
					bx::read(reader, group.m_obb);

					bgfx::read(reader, m_layout);
					uint16_t stride = m_layout.getStride();

					uint16_t numVertices;
					bx::read(reader, numVertices);
					const bgfx::Memory* mem = bgfx::alloc(numVertices*stride);
					bx::read(reader, mem->data, mem->size);

					group.m_vbh = bgfx::createVertexBuffer(mem, m_layout);
				}
				break;

				case BGFX_CHUNK_MAGIC_IB:
				{
					uint32_t numIndices;
					bx::read(reader, numIndices);
					const bgfx::Memory* mem = bgfx::alloc(numIndices * 2);
					bx::read(reader, mem->data, mem->size);
					group.m_ibh = bgfx::createIndexBuffer(mem);
				}
				break;

				case BGFX_CHUNK_MAGIC_PRI:
				{
					uint16_t len;
					bx::read(reader, len);

					// read material name in
					std::string material;
					material.resize(len);
					bx::read(reader, const_cast<char*>(material.c_str()), len);

					// convert material definition over and load texture
					MaterialDef& matDef = mtlDefs[material];
					group.m_material.m_diffuseTint = glm::vec3(matDef.m_diffuseTint[0], matDef.m_diffuseTint[1], matDef.m_diffuseTint[2]);
					group.m_material.m_specTint = glm::vec3(matDef.m_specTint[0], matDef.m_specTint[1], matDef.m_specTint[2]);
					group.m_material.m_roughness = pow(2.0f / (2.0f + matDef.m_specExp), 0.25f);

					uint32_t samplerFlags = BGFX_SAMPLER_MIN_ANISOTROPIC;

					// load textures
					group.m_material.m_diffuseMap = loadTexturePriv(matDef.m_diffuseMap, "white.png", uint32_t(samplerFlags | BGFX_TEXTURE_SRGB));
					group.m_material.m_nmlMap = loadTexturePriv(matDef.m_bmpMap, "nml.tga", samplerFlags);
					group.m_material.m_roughnessMap = loadTexturePriv(matDef.m_roughnessMap, "white.png", samplerFlags);
					group.m_material.m_metallicMap = loadTexturePriv(matDef.m_metallicMap, "black.png", samplerFlags);

					// read primitive data
					uint16_t num;
					bx::read(reader, num);

					for (uint32_t ii = 0; ii < num; ++ii)
					{
						bx::read(reader, len);

						std::string name;
						name.resize(len);
						bx::read(reader, const_cast<char*>(name.c_str()), len);

						Primitive prim;
						bx::read(reader, prim.m_startIndex);
						bx::read(reader, prim.m_numIndices);
						bx::read(reader, prim.m_startVertex);
						bx::read(reader, prim.m_numVertices);
						bx::read(reader, prim.m_sphere);
						bx::read(reader, prim.m_aabb);
						bx::read(reader, prim.m_obb);

						group.m_prims.push_back(prim);
					}

					m_groups.push_back(group);
					group.reset();
				}
				break;

				default:
					DBG("%08x at %d", chunk, bx::skip(reader, 0));
					break;
				}
			}

			bx::close(reader);
		}

		void unload()
		{
			for (GroupArray::const_iterator it = m_groups.cbegin(), itEnd = m_groups.cend(); it != itEnd; ++it)
			{
				const Group& group = *it;
				bgfx::destroy(group.m_vbh);

				if (group.m_ibh.idx != bgfx::kInvalidHandle)
				{
					bgfx::destroy(group.m_ibh);
				}
			}
			m_groups.clear();
		}

		void submit(GlobalRenderingData& _rdata, uint8_t _viewId, float* _mtx, bgfx::ProgramHandle _program, /*const LightMaps& colorMaps,*/ const RenderState& _renderState)
		{
			for (GroupArray::const_iterator it = m_groups.begin(); it != m_groups.end(); ++it)
			{
				const Group& group = *it;

				_rdata.m_uniforms.submitPerDrawUniforms();

				bgfx::setTransform(_mtx);
				bgfx::setIndexBuffer(group.m_ibh);
				bgfx::setVertexBuffer(0, group.m_vbh);

				/*bgfx::setTexture(0, _rdata.m_uColorMap, colorMaps.m_colorMap);
				bgfx::setTexture(1, _rdata.m_uFilteredMap, colorMaps.m_filteredMap);*/

				bgfx::setStencil(_renderState.m_fstencil, _renderState.m_bstencil);
				bgfx::setState(_renderState.m_state, _renderState.m_blendFactorRgba);

				bgfx::submit(_viewId, _program);
			}
		}

		bgfx::VertexLayout m_layout;
		typedef std::vector<Group> GroupArray;
		GroupArray m_groups;

		static std::map < std::string, bgfx::TextureHandle> m_textureCache;
	};

	struct Model
	{
		void loadModel(const char* _filename)
		{
			m_mesh.load(_filename);
		}

		void loadModel(const void* _vertices, uint32_t _numVertices, const bgfx::VertexLayout _layout, const uint16_t* _indices, uint32_t _numIndices)
		{
			m_mesh.load(_vertices, _numVertices, _layout, _indices, _numIndices);
		}

		void unload()
		{
			m_mesh.unload();
		}

		void submit(GlobalRenderingData& _rdata, uint8_t _viewId, bgfx::ProgramHandle _program, /*const LightMaps& colorMaps,*/ const RenderState& _renderState)
		{
			m_mesh.submit(_rdata, _viewId, glm::value_ptr(m_transform), _program, /*colorMaps,*/ _renderState);
		}

		glm::mat4 m_transform;
		Mesh m_mesh;
	};
}

struct PosColorTexCoord0Vertex
{
	float m_x;
	float m_y;
	float m_z;
	uint32_t m_rgba;
	float m_u;
	float m_v;

	static void init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.end();
	}

	static bgfx::VertexLayout ms_layout;
};

struct LightData
{
	LightData() : textureIdx(0), intensity(4.0f), twoSided(false) { }

	glm::vec3 rotation;
	glm::vec2 scale;
	glm::vec3 position;
	glm::vec3 color;
	uint32_t textureIdx;

	float intensity;
	bool  twoSided;
};

inline uint32_t packUint32(uint8_t _x, uint8_t _y, uint8_t _z, uint8_t _w)
{
	union
	{
		uint32_t ui32;
		uint8_t arr[4];
	} un;

	un.arr[0] = _x;
	un.arr[1] = _y;
	un.arr[2] = _z;
	un.arr[3] = _w;

	return un.ui32;
}

inline uint32_t packF4u(float _x, float _y, float _z, float _w)
{
	const uint8_t xx = uint8_t(_x*127.0f + 128.0f);
	const uint8_t yy = uint8_t(_y*127.0f + 128.0f);
	const uint8_t zz = uint8_t(_z*127.0f + 128.0f);
	const uint8_t ww = uint8_t(_w*127.0f + 128.0f);
	return packUint32(xx, yy, zz, ww);
}

#endif
