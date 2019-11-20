$input v_position, v_normal, v_tangent, v_bitangent, v_texcoord

#include "../common/common.sh"
#include "../common/pbr_helpers.sh"

#define MAX_SLOPE_OFFSET 2.0
#define NUM_CASCADES 4

uniform vec4 u_shadowMapParams;
#define u_shadowBias u_shadowMapParams.x
#define u_slopeScaleBias u_shadowMapParams.y
#define u_normalOffsetFactor u_shadowMapParams.z
#define u_shadowMapTexelSize u_shadowMapParams.w

uniform vec4 u_cameraPos;
uniform vec4 u_directionalLightParams[2];
#define u_lightColor u_directionalLightParams[0].xyz
#define u_lightIntesity u_directionalLightParams[0].w
#define u_lightDir u_directionalLightParams[1].xyz
uniform vec4 u_samplingDisk[8];
uniform vec4 u_cascadeBounds[NUM_CASCADES];
#define u_diskSize u_cascadeBounds[0].w
uniform mat4 u_lightViewProj[NUM_CASCADES];

SAMPLER2D(s_shadowMap_1, 5);
SAMPLER2D(s_shadowMap_2, 6);
SAMPLER2D(s_shadowMap_3, 7);
SAMPLER2D(s_shadowMap_4, 8);
SAMPLER2D(s_randomTexture, 9);


SAMPLER2D(s_baseColor, 0);
SAMPLER2D(s_normal, 1);
SAMPLER2D(s_metallicRoughness, 2);
SAMPLER2D(s_emissive, 3);
SAMPLER2D(s_occlusion, 4);
uniform vec4 u_factors[3];
#define u_baseColorFactor u_factors[0]
#define u_emissiveFactor u_factors[1]
#define u_alphaCutoff u_factors[2].x
#define u_metallicFactor u_factors[2].y
#define u_roughnessFactor u_factors[2].z
