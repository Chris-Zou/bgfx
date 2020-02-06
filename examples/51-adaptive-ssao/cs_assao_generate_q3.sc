/*
 * Copyright 2020 Zou Pan Pan. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include "bgfx_compute.sh" 
#include "uniforms.sh"

#define INTELSSAO_MAIN_DISK_SAMPLE_COUNT (32)
CONST(vec4 g_samplePatternMain[INTELSSAO_MAIN_DISK_SAMPLE_COUNT]) =
{
   { 0.78488064,  0.56661671,  1.500000, -0.126083},    { 0.26022232, -0.29575172,  1.500000, -1.064030},    { 0.10459357,  0.08372527,  1.110000, -2.730563},    {-0.68286800,  0.04963045,  1.090000, -0.498827},
   {-0.13570161, -0.64190155,  1.250000, -0.532765},    {-0.26193795, -0.08205118,  0.670000, -1.783245},    {-0.61177456,  0.66664219,  0.710000, -0.044234},    { 0.43675563,  0.25119025,  0.610000, -1.167283},
   { 0.07884444,  0.86618668,  0.640000, -0.459002},    {-0.12790935, -0.29869005,  0.600000, -1.729424},    {-0.04031125,  0.02413622,  0.600000, -4.792042},    { 0.16201244, -0.52851415,  0.790000, -1.067055},
   {-0.70991218,  0.47301072,  0.640000, -0.335236},    { 0.03277707, -0.22349690,  0.600000, -1.982384},    { 0.68921727,  0.36800742,  0.630000, -0.266718},    { 0.29251814,  0.37775412,  0.610000, -1.422520},
   {-0.12224089,  0.96582592,  0.600000, -0.426142},    { 0.11071457, -0.16131058,  0.600000, -2.165947},    { 0.46562141, -0.59747696,  0.600000, -0.189760},    {-0.51548797,  0.11804193,  0.600000, -1.246800},
   { 0.89141309, -0.42090443,  0.600000,  0.028192},    {-0.32402530, -0.01591529,  0.600000, -1.543018},    { 0.60771245,  0.41635221,  0.600000, -0.605411},    { 0.02379565, -0.08239821,  0.600000, -3.809046},
   { 0.48951152, -0.23657045,  0.600000, -1.189011},    {-0.17611565, -0.81696892,  0.600000, -0.513724},    {-0.33930185, -0.20732205,  0.600000, -1.698047},    {-0.91974425,  0.05403209,  0.600000,  0.062246},
   {-0.15064627, -0.14949332,  0.600000, -1.896062},    { 0.53180975, -0.35210401,  0.600000, -0.758838},    { 0.41487166,  0.81442589,  0.600000, -0.505648},    {-0.24106961, -0.32721516,  0.600000, -1.665244}
};

CONST(uint g_numTaps[5]) = { 3, 5, 12, 0, 0 };

#define SSAO_TILT_SAMPLES_ENABLE_AT_QUALITY_PRESET                      (99)        
#define SSAO_TILT_SAMPLES_AMOUNT                                        (0.4)

#define SSAO_HALOING_REDUCTION_ENABLE_AT_QUALITY_PRESET                 (1)         
#define SSAO_HALOING_REDUCTION_AMOUNT                                   (0.6)       

#define SSAO_NORMAL_BASED_EDGES_ENABLE_AT_QUALITY_PRESET                (2)         
#define SSAO_NORMAL_BASED_EDGES_DOT_THRESHOLD                           (0.5)       

#define SSAO_DETAIL_AO_ENABLE_AT_QUALITY_PRESET                         (1)         

#define SSAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET                        (2)     
#define SSAO_DEPTH_MIPS_GLOBAL_OFFSET                                   (-4.3)

#define SSAO_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET                 (1)     

#define SSAO_REDUCE_RADIUS_NEAR_SCREEN_BORDER_ENABLE_AT_QUALITY_PRESET  (99) 

SAMPLER2D(s_viewspaceDepthSource,  0); 
SAMPLER2D(s_viewspaceDepthSourceMirror,  1); 
IMAGE2D_RO(s_normalmapSource, rgba8, 2);
BUFFER_RO(s_loadCounter, uint, 3); 
SAMPLER2D(s_importanceMap,  4); 
IMAGE2D_ARRAY_RO(s_baseSSAO, rg8, 5);
IMAGE2D_ARRAY_WR(s_target, rg8, 6);

float PackEdges( vec4 edgesLRTB )
{
    edgesLRTB = round( saturate( edgesLRTB ) * 3.05 );
    return dot( edgesLRTB, vec4( 64.0 / 255.0, 16.0 / 255.0, 4.0 / 255.0, 1.0 / 255.0 ) ) ;
}

vec3 NDCToViewspace( vec2 pos, float viewspaceDepth )
{
    vec3 ret;

    ret.xy = (u_ndcToViewMul * pos.xy + u_ndcToViewAdd) * viewspaceDepth;

    ret.z = viewspaceDepth;

    return ret;
}


void CalculateRadiusParameters( const float pixCenterLength, const vec2 pixelDirRBViewspaceSizeAtCenterZ, out float pixLookupRadiusMod, out float effectRadius, out float falloffCalcMulSq )
{
	effectRadius = u_effectRadius;

	const float tooCloseLimitMod = saturate(pixCenterLength * u_effectSamplingRadiusNearLimitRec) * 0.8 + 0.2;
	effectRadius *= tooCloseLimitMod;

	pixLookupRadiusMod = (0.85 * effectRadius) / pixelDirRBViewspaceSizeAtCenterZ.x;

	falloffCalcMulSq = -1.0f / (effectRadius * effectRadius);
}

vec4 CalculateEdges(const float centerZ, const float leftZ, const float rightZ, const float topZ, const float bottomZ)
{
	vec4 edgesLRTB = vec4( leftZ, rightZ, topZ, bottomZ ) - centerZ;
    vec4 edgesLRTBSlopeAdjusted = edgesLRTB + edgesLRTB.yxwz;
    edgesLRTB = min( abs( edgesLRTB ), abs( edgesLRTBSlopeAdjusted ) );
    return saturate( ( 1.3 - edgesLRTB / (centerZ * 0.040) ) );
}

vec3 DecodeNormal( vec3 encodedNormal )
{
    vec3 normal = encodedNormal * u_normalsUnpackMul.xxx + u_normalsUnpackAdd.xxx;

#if SSAO_ENABLE_NORMAL_WORLD_TO_VIEW_CONVERSION
	normal = vec3( dot(normal, u_normalsWorldToViewspaceMatrix0.xyz),
					dot(normal, u_normalsWorldToViewspaceMatrix1.xyz),
					dot(normal, u_normalsWorldToViewspaceMatrix2.xyz));
#endif

    return normal;
}

vec3 LoadNormal( ivec2 pos )
{
    vec3 encodedNormal = imageLoad(s_normalmapSource, pos).xyz;
    return DecodeNormal( encodedNormal );
}

vec3 LoadNormal( ivec2 pos, ivec2 offset )
{
    vec3 encodedNormal = imageLoad(s_normalmapSource, pos + offset ).xyz;
    return DecodeNormal( encodedNormal );
}

float CalculatePixelObscurance( vec3 pixelNormal, vec3 hitDelta, float falloffCalcMulSq )
{
  float lengthSq = dot( hitDelta, hitDelta );
  float NdotD = dot(pixelNormal, hitDelta) / sqrt(lengthSq);

  float falloffMult = max( 0.0, lengthSq * falloffCalcMulSq + 1.0 );

  return max( 0, NdotD - u_effectHorizonAngleThreshold ) * falloffMult;
}

void SSAOTapInner(inout float obscuranceSum, inout float weightSum, const vec2 samplingUV, const float mipLevel, const vec3 pixCenterPos, const vec3 negViewspaceDir,vec3 pixelNormal, const float falloffCalcMulSq, const float weightMod, const int dbgTapIndex)
{
    // get depth at sample
    float viewspaceSampleZ = texture2DLod(s_viewspaceDepthSource, samplingUV.xy, mipLevel ).x;

    // convert to viewspace
    vec3 hitPos = NDCToViewspace( samplingUV.xy, viewspaceSampleZ ).xyz;
    vec3 hitDelta = hitPos - pixCenterPos;

    float obscurance = CalculatePixelObscurance( pixelNormal, hitDelta, falloffCalcMulSq );
    float weight = 1.0;
 
    //float reduct = max( 0, dot( hitDelta, negViewspaceDir ) );
    float reduct = max( 0, -hitDelta.z ); // cheaper, less correct version
    reduct = saturate( reduct * u_negRecEffectRadius + 2.0 ); // saturate( 2.0 - reduct / u_effectRadius );
    weight = SSAO_HALOING_REDUCTION_AMOUNT * reduct + (1.0 - SSAO_HALOING_REDUCTION_AMOUNT);
    weight *= weightMod;
    obscuranceSum += obscurance * weight;
    weightSum += weight;
}

void SSAOTap(inout float obscuranceSum, inout float weightSum, const int tapIndex, const mat2 rotScale, const vec3 pixCenterPos, const vec3 negViewspaceDir, vec3 pixelNormal, const vec2 normalizedScreenPos, const float mipOffset, const float falloffCalcMulSq, float weightMod, vec2 normXY, float normXYLength)
{
	vec2  sampleOffset;
    float samplePow2Len;

	{
        vec4 newSample = g_samplePatternMain[tapIndex];
        sampleOffset    = mul( rotScale, newSample.xy );
        samplePow2Len   = newSample.w;                      // precalculated, same as: samplePow2Len = log2( length( newSample.xy ) );
        weightMod *= newSample.z;
    }

	sampleOffset = round(sampleOffset);

	float mipLevel = samplePow2Len + mipOffset;
	vec2 samplingUV = sampleOffset * u_viewport2xPixelSize + normalizedScreenPos;

	SSAOTapInner(obscuranceSum, weightSum, samplingUV, mipLevel, pixCenterPos, negViewspaceDir, pixelNormal, falloffCalcMulSq, weightMod, tapIndex * 2);
	vec2 sampleOffsetMirroredUV    = -sampleOffset;

	vec2 samplingMirroredUV = sampleOffsetMirroredUV * u_viewport2xPixelSize + normalizedScreenPos;

    SSAOTapInner(obscuranceSum, weightSum, samplingMirroredUV, mipLevel, pixCenterPos, negViewspaceDir, pixelNormal, falloffCalcMulSq, weightMod, tapIndex * 2 + 1);
}

void GenerateSSAOShadowsInternal( out float outShadowTerm, out vec4 outEdges, out float outWeight, const vec2 SVPos)
{
	vec2 SVPosRounded = trunc(SVPos);
	uvec2 SVPosui = uvec2(SVPosRounded);

	const uint numberOfTaps = 0;
	float pixZ, pixLZ, pixRZ, pixTZ, pixBZ;

	vec4 valuesUL = textureGather(s_viewspaceDepthSourceMirror, SVPosRounded * u_halfViewportPixelSize);
	vec4 valuesBR = textureGatherOffset(s_viewspaceDepthSourceMirror, SVPosRounded * u_halfViewportPixelSize, ivec2(1, 1));

	pixZ = valuesUL.y;

	pixLZ = valuesUL.x;
	pixTZ = valuesUL.z;
	pixRZ = valuesBR.z;
	pixBZ = valuesBR.x;

	vec2 normalizedScreenPos = SVPosRounded * u_viewport2xPixelSize + u_viewport2xPixelSize_x_025;
	vec3 pixCenterPos = NDCToViewspace(normalizedScreenPos, pixZ);

	uvec2 fullResCoord = uvec2(SVPosui * 2 + u_perPassFullResCoordOffset.xy);
	vec3 pixelNormal = LoadNormal(ivec2(fullResCoord));

	const vec2 pixelDirRBViewspaceSizeAtCenterZ = NDCToViewspace( normalizedScreenPos.xy + u_viewportPixelSize.xy, pixCenterPos.z ).xy - pixCenterPos.xy;

	float pixLookupRadiusMod;
	float falloffCalcMulSq;

	float effectViewspaceRadius;
	CalculateRadiusParameters(length(pixCenterPos), pixelDirRBViewspaceSizeAtCenterZ, pixLookupRadiusMod, effectViewspaceRadius, falloffCalcMulSq);

	mat2 rotScale;
	{
		uint pseudoRandomIndex = uint( SVPosRounded.y * 2 + SVPosRounded.x ) % 5;

		vec4 rs = u_patternRotScaleMatrices( pseudoRandomIndex );
        rotScale = mat2( rs.x * pixLookupRadiusMod, rs.y * pixLookupRadiusMod, rs.z * pixLookupRadiusMod, rs.w * pixLookupRadiusMod );
	}

	float obscuranceSum = 0.0;
    float weightSum = 0.0;

	vec4 edgesLRTB = vec4( 1.0, 1.0, 1.0, 1.0 );

	pixCenterPos *= u_depthPrecisionOffsetMod;

	edgesLRTB = CalculateEdges( pixZ, pixLZ, pixRZ, pixTZ, pixBZ );

	vec3 viewspaceDirZNormalized = vec3(pixCenterPos.xy / pixCenterPos.zz, 1.0);

	vec3 pixLDelta  = vec3( -pixelDirRBViewspaceSizeAtCenterZ.x, 0.0, 0.0 ) + viewspaceDirZNormalized * (pixLZ - pixCenterPos.z); // very close approximation of: vec3 pixLPos  = NDCToViewspace( normalizedScreenPos + vec2( -u_halfViewportPixelSize.x, 0.0 ), pixLZ ).xyz - pixCenterPos.xyz;
    vec3 pixRDelta  = vec3( +pixelDirRBViewspaceSizeAtCenterZ.x, 0.0, 0.0 ) + viewspaceDirZNormalized * (pixRZ - pixCenterPos.z); // very close approximation of: vec3 pixRPos  = NDCToViewspace( normalizedScreenPos + vec2( +u_halfViewportPixelSize.x, 0.0 ), pixRZ ).xyz - pixCenterPos.xyz;
    vec3 pixTDelta  = vec3( 0.0, -pixelDirRBViewspaceSizeAtCenterZ.y, 0.0 ) + viewspaceDirZNormalized * (pixTZ - pixCenterPos.z); // very close approximation of: vec3 pixTPos  = NDCToViewspace( normalizedScreenPos + vec2( 0.0, -u_halfViewportPixelSize.y ), pixTZ ).xyz - pixCenterPos.xyz;
    vec3 pixBDelta  = vec3( 0.0, +pixelDirRBViewspaceSizeAtCenterZ.y, 0.0 ) + viewspaceDirZNormalized * (pixBZ - pixCenterPos.z); // very close approximation of: vec3 pixBPos  = NDCToViewspace( normalizedScreenPos + vec2( 0.0, +u_halfViewportPixelSize.y ), pixBZ ).xyz - pixCenterPos.xyz;


	const float rangeReductionConst = 4.0f;
	const float modifiedFalloffCalcMulSq = rangeReductionConst * falloffCalcMulSq;

	vec4 additionalObscurance;
	additionalObscurance.x = CalculatePixelObscurance(pixelNormal, pixLDelta, modifiedFalloffCalcMulSq);
	additionalObscurance.y = CalculatePixelObscurance(pixelNormal, pixRDelta, modifiedFalloffCalcMulSq);
	additionalObscurance.z = CalculatePixelObscurance(pixelNormal, pixTDelta, modifiedFalloffCalcMulSq);
	additionalObscurance.w = CalculatePixelObscurance(pixelNormal, pixBDelta, modifiedFalloffCalcMulSq);

	obscuranceSum += u_detailAOStrength * dot( additionalObscurance, edgesLRTB );

	vec3 neighbourNormalL = LoadNormal(ivec2(fullResCoord), ivec2(-2, 0));
	vec3 neighbourNormalR = LoadNormal(ivec2(fullResCoord), ivec2(2, 0));
	vec3 neighbourNormalT = LoadNormal(ivec2(fullResCoord), ivec2(0, -2));
	vec3 neighbourNormalB = LoadNormal(ivec2(fullResCoord), ivec2(0, 2));

	const float dotThreshold = SSAO_NORMAL_BASED_EDGES_DOT_THRESHOLD;

	vec4 normalEdgesLRTB;
	normalEdgesLRTB.x = saturate( (dot( pixelNormal, neighbourNormalL ) + dotThreshold ) );
    normalEdgesLRTB.y = saturate( (dot( pixelNormal, neighbourNormalR ) + dotThreshold ) );
    normalEdgesLRTB.z = saturate( (dot( pixelNormal, neighbourNormalT ) + dotThreshold ) );
    normalEdgesLRTB.w = saturate( (dot( pixelNormal, neighbourNormalB ) + dotThreshold ) );

	edgesLRTB *= normalEdgesLRTB;

	const float globalMipOffset = SSAO_DEPTH_MIPS_GLOBAL_OFFSET;
	float mipOffset = log2( pixLookupRadiusMod ) + globalMipOffset;

	vec2 normXY = vec2(pixelNormal.x, pixelNormal.y);
	float normXYLength = length(normXY);
	normXY /= vec2(normXYLength, -normXYLength);
	normXYLength *= SSAO_TILT_SAMPLES_AMOUNT;

	const vec3 negViewspaceDir = -normalize(pixCenterPos);

	vec2 fullResUV = normalizedScreenPos + u_perPassFullResUVOffset.xy;
	float importance = texture2DLod(s_importanceMap, fullResUV, 0.0).x;

	obscuranceSum *= (SSAO_ADAPTIVE_TAP_BASE_COUNT / float(SSAO_MAX_TAPS)) + (importance * SSAO_ADAPTIVE_TAP_FLEXIBLE_COUNT / float(SSAO_MAX_TAPS));

	vec2 baseValues = imageLoad(s_baseSSAO, ivec3(SVPosui, u_passIndex)).xy;
	weightSum += baseValues.y * (float(SSAO_ADAPTIVE_TAP_BASE_COUNT) * 4.0f);
	obscuranceSum += (baseValues.x) * weightSum;

	float edgeCount = dot(1.0 - edgesLRTB, vec4(1.0f, 1.0f, 1.0f, 1.0f));

	float avgTotalImportance = float(s_loadCounter[0]) * u_loadCounterAvgDiv;

	float importanceLimiter = saturate(u_adaptiveSampleCountLimit / avgTotalImportance);
	importance *= importanceLimiter;

	float additionalSampleCountFlt = SSAO_ADAPTIVE_TAP_FLEXIBLE_COUNT * importance;

	const float blendRange = 3.0f;
	const float blendRangeInv = 1.0 / blendRange;

	additionalSampleCountFlt += 0.5f;
	uint additionalSamples = uint(additionalSampleCountFlt);
	uint additionalSamplesTo = min(SSAO_MAX_TAPS, additionalSamples + SSAO_ADAPTIVE_TAP_BASE_COUNT);

	for(uint i = SSAO_ADAPTIVE_TAP_BASE_COUNT; i < additionalSamplesTo; i++)
	{
		additionalSampleCountFlt -= 1.0f;
		float weightMod = saturate(additionalSampleCountFlt * blendRangeInv);
		SSAOTap(obscuranceSum, weightSum, int(i), rotScale, pixCenterPos, negViewspaceDir, pixelNormal, normalizedScreenPos, mipOffset, falloffCalcMulSq, weightMod, normXY, normXYLength);
	}

	float obscurance = obscuranceSum / weightSum;

	float fadeOut = saturate(pixCenterPos.z * u_effectFadeOutMul + u_effectFadeOutAdd);

	float edgeFadeoutFactor = saturate((1.0 - edgesLRTB.x - edgesLRTB.y) * 0.35) + saturate((1.0 - edgesLRTB.z - edgesLRTB.w) * 0.35);

	fadeOut *= saturate(1.0 - edgeFadeoutFactor);

	obscurance = u_effectShadowStrength * obscurance;
	obscurance = min(obscurance, u_effectShadowClamp);

	obscurance *= fadeOut;

	float occlusion = 1.0 - obscurance;

	occlusion = pow(saturate(occlusion), u_effectShadowPow);

	outShadowTerm = occlusion;
	outEdges = edgesLRTB;
	outWeight = weightSum;
}

NUM_THREADS(8, 8, 1)
void main()
{
	uvec2 dtID = uvec2(gl_GlobalInvocationID.xy) + uvec2(u_rect.xy);
	if(all(lessThan(dtID.xy, u_rect.zw)))
	{
		float outShadowTerm;
		float outWeight;
		vec4 outEdges;
		GenerateSSAOShadowsInternal(outShadowTerm, outEdges, outWeight, vec2(dtID.xy));
		vec2 out0;
		out0.x = outShadowTerm;

		out0.y = PackEdges(outEdges);

		imageStore(s_target, ivec3(dtID.xy, u_layer), out0.xyyy);
	}
}
