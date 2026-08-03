#ifndef RADIANCE_HINTS_COMMON_FXC_H
#define RADIANCE_HINTS_COMMON_FXC_H

#include "deferred_global_common.h"

#define RH_PI 3.14159265358979323846f
#define RH_FOUR_PI 12.56637061435917295385f

// L1 real spherical harmonics. Coefficient order is X, Y, Z, DC.
float4 RH_ProjectDirection( float3 direction )
{
	direction = normalize( direction );
	return float4( direction * 0.4886025119f, 0.2820947918f );
}

// Convolution of L1 SH radiance with the clamped-cosine diffuse kernel.
// Our volume stores ray travel direction, therefore callers normally pass -normal.
float4 RH_DiffuseBasis( float3 direction )
{
	direction = normalize( direction );
	return float4( direction * 1.0233267079f, 0.8862269255f );
}

float3 RH_EvaluateRadiance( float4 shR, float4 shG, float4 shB, float3 direction )
{
	float4 basis = RH_ProjectDirection( direction );
	return max( float3( dot( shR, basis ), dot( shG, basis ), dot( shB, basis ) ), 0.0f );
}

float3 RH_EvaluateDiffuse( float4 shR, float4 shG, float4 shB, float3 surfaceNormal )
{
	float4 basis = RH_DiffuseBasis( -surfaceNormal );
	return max( float3( dot( shR, basis ), dot( shG, basis ), dot( shB, basis ) ), 0.0f );
}

float RH_InsideVolume( float3 uvw )
{
	float3 lowMask = step( 0.0f, uvw );
	float3 highMask = step( uvw, 1.0f );
	return lowMask.x * lowMask.y * lowMask.z * highMask.x * highMask.y * highMask.z;
}

float RH_BoundaryFade( float3 uvw )
{
	float3 edgeDistance = min( uvw, 1.0f - uvw );
	float nearestEdge = min( edgeDistance.x, min( edgeDistance.y, edgeDistance.z ) );
	return saturate( nearestEdge * RH_VOLUME_SIZE_F * 0.75f );
}

// Emulates trilinear filtering on a 2D atlas containing Z slices from left to right.
// Hardware bilinear filtering is used inside each XY slice, followed by manual Z lerp.
float4 RH_SampleAtlas( sampler volumeSampler, float3 uvw )
{
	uvw = saturate( uvw );

	const float sliceWidth = 1.0f / RH_VOLUME_SIZE_F;
	const float atlasTexel = 1.0f / RH_ATLAS_WIDTH_F;
	const float volumeTexel = 1.0f / RH_VOLUME_SIZE_F;
	const float innerSliceWidth = atlasTexel * ( RH_VOLUME_SIZE_F - 1.0f );
	const float innerVolumeHeight = volumeTexel * ( RH_VOLUME_SIZE_F - 1.0f );

	float zPosition = uvw.z * ( RH_VOLUME_SIZE_F - 1.0f );
	float zSlice0 = floor( zPosition );
	float zSlice1 = min( zSlice0 + 1.0f, RH_VOLUME_SIZE_F - 1.0f );
	float zBlend = frac( zPosition );

	float xInSlice = atlasTexel * 0.5f + uvw.x * innerSliceWidth;
	float sampleY = volumeTexel * 0.5f + uvw.y * innerVolumeHeight;

	float2 uv0 = float2( zSlice0 * sliceWidth + xInSlice, sampleY );
	float2 uv1 = float2( zSlice1 * sliceWidth + xInSlice, sampleY );

	return lerp( tex2D( volumeSampler, uv0 ), tex2D( volumeSampler, uv1 ), zBlend );
}

#endif
