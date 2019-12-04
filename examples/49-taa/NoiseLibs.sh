#ifndef _NOISELIBS_H_
#define _NOISELIBS_H_

float nrand( float2 n ) {
	return frac( sin(dot(n.xy, float2(12.9898f, 78.233f)))* 43758.5453f );
}
float2 nrand2( float2 n ) {
	return frac( sin(dot(n.xy, float2(12.9898f, 78.233f)))* float2(43758.5453f, 28001.8384f) );
}
float3 nrand3( float2 n ) {
	return frac( sin(dot(n.xy, float2(12.9898f, 78.233f)))* float3(43758.5453f, 28001.8384f, 50849.4141f ) );
}
float4 nrand4( float2 n ) {
	return frac( sin(dot(n.xy, float2(12.9898f, 78.233f)))* float4(43758.5453f, 28001.8384f, 50849.4141f, 12996.89f) );
}

//note: signed random, float=[-1, 1]
float srand( float2 n ) {
	return nrand( n ) * 2 - 1;
}
float2 srand2( float2 n ) {
	return nrand2( n ) * 2 - 1;
}
float3 srand3( float2 n ) {
	return nrand3( n ) * 2 - 1;
}
float4 srand4( float2 n ) {
	return nrand4( n ) * 2 - 1;
}

#endif // _NOISELIBS_H_
