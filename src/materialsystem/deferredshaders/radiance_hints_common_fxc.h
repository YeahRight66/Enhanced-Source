#ifndef RADIANCE_HINTS_COMMON_FXC_H
#define RADIANCE_HINTS_COMMON_FXC_H

#include "radiance_hints_config.h"

#define RH_PI              3.14159265358979323846f
#define RH_EPSILON         1.0e-7f
#define RH_FP16_SAFE_MAX   60000.0f

float3 RH_SafeNormalize( float3 value, float3 fallbackDirection )
{
    float lengthSquared = dot( value, value );
    float valid = step( RH_EPSILON, lengthSquared );
    float3 normalizedValue = value * rsqrt( max( lengthSquared, RH_EPSILON ) );
    return lerp( fallbackDirection, normalizedValue, valid );
}

float2 RH_Rotate2D( float2 value, float sineValue, float cosineValue )
{
    return float2(
        value.x * cosineValue - value.y * sineValue,
        value.x * sineValue + value.y * cosineValue );
}

float RH_Hash13( float3 value )
{
    value = frac( value * 0.1031f );
    value += dot( value, value.yzx + 33.33f );
    return frac( ( value.x + value.y ) * value.z );
}

float3 RH_ClampRadiance( float3 value, float maximumValue )
{
    return min( max( value, 0.0f ), maximumValue );
}

// Energy-bounds L1 SH so directional terms cannot exceed the DC term and
// create negative lobes, hue inversion, or FP16 fireflies.
float4 RH_LimitSH( float4 sh, float directionalRatio )
{
    sh.w = max( sh.w, 0.0f );
    float directionalLength = length( sh.xyz );
    float maximumDirectionalLength = sh.w * directionalRatio + 1.0e-5f;
    sh.xyz *= min( 1.0f, maximumDirectionalLength / max( directionalLength, 1.0e-5f ) );
    return clamp( sh, -RH_FP16_SAFE_MAX, RH_FP16_SAFE_MAX );
}

float4 RH_LimitRadianceSH( float4 sh )
{
    // A purely directional L1 projection reaches |L1|/DC = sqrt(3).  RH9's
    // 1.45 hard clamp unnecessarily flattened directional sunlight.  Keep a
    // small FP16 safety margin but preserve essentially the full legal L1 range.
    return RH_LimitSH( sh, 1.70f );
}

float4 RH_LimitVisibilitySH( float4 sh )
{
    return RH_LimitSH( sh, 1.70f );
}

// L1 real spherical harmonics. Coefficient order: X, Y, Z, DC.
float4 RH_ProjectDirection( float3 direction )
{
    direction = RH_SafeNormalize( direction, float3( 0.0f, 0.0f, 1.0f ) );
    return float4( direction * 0.4886025119f, 0.2820947918f );
}

float4 RH_DiffuseBasis( float3 direction )
{
    direction = RH_SafeNormalize( direction, float3( 0.0f, 0.0f, 1.0f ) );
    return float4( direction * 1.0233267079f, 0.8862269255f );
}

// L1 SH projection of a constant-radiance hemisphere whose outgoing/travel
// directions are centred on hemisphereDirection. Used by hemisphere skylight
// and Lambertian surface re-emission.
float4 RH_ProjectHemisphere( float3 hemisphereDirection )
{
    hemisphereDirection = RH_SafeNormalize( hemisphereDirection, float3( 0.0f, 0.0f, 1.0f ) );
    return float4( hemisphereDirection * 1.5349900619f, 1.7724538509f );
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

float RH_EvaluateVisibility( float4 visibilitySH, float3 direction )
{
    // Visibility injection stores an average of SH basis samples. Reconstruct
    // it as a spherical moment with the matching 4*PI normalization instead of
    // the historical 0.4*PI tuning constant. Strength is calibrated by the CVar.
    return saturate( dot( visibilitySH, RH_ProjectDirection( direction ) ) * 12.5663706144f );
}

float RH_EvaluateVisibilityTwoSided( float4 visibilitySH, float3 direction )
{
    // Blocker surfaces are two-sided for transport. Evaluating both directions
    // makes a coarse cell block a segment regardless of which side sampled it.
    return max(
        RH_EvaluateVisibility( visibilitySH, direction ),
        RH_EvaluateVisibility( visibilitySH, -direction ) );
}

float3 RH_DominantTravelDirection( float4 shR, float4 shG, float4 shB, float3 fallbackDirection )
{
    float3 directional = shR.xyz * 0.2126f + shG.xyz * 0.7152f + shB.xyz * 0.0722f;
    return RH_SafeNormalize( directional, fallbackDirection );
}

float RH_RadianceEnergy( float4 shR, float4 shG, float4 shB )
{
    return max( shR.w * 0.2126f + shG.w * 0.7152f + shB.w * 0.0722f, 0.0f );
}


float RH_MetaConfidence( float4 meta )
{
    return saturate( meta.x ) * saturate( meta.w );
}

float RH_MetaSurfaceProximity( float4 meta )
{
    // RH10: additive, all-phase near-surface moment. 0=open/far, 1=near a
    // well-supported injected surface. This replaces RH9's phase-0-only min.
    return saturate( meta.y );
}

float RH_EnergySimilarity( float centerEnergy, float sampleEnergy, float scale )
{
    // Compare logarithmic energy so the same threshold works in dark rooms and
    // near bright sun-bounce cells. Raw normalized differences over-reject
    // useful gradients while still allowing isolated fireflies to spread.
    float centerLog = log2( 1.0f + max( centerEnergy, 0.0f ) * 16.0f );
    float sampleLog = log2( 1.0f + max( sampleEnergy, 0.0f ) * 16.0f );
    return exp2( -abs( centerLog - sampleLog ) * max( scale, 0.0f ) );
}

float RH_DirectionalConfidence( float4 shR, float4 shG, float4 shB )
{
    float3 directional = shR.xyz * 0.2126f + shG.xyz * 0.7152f + shB.xyz * 0.0722f;
    float dc = RH_RadianceEnergy( shR, shG, shB );
    return saturate( length( directional ) / max( dc * 1.7320508f, 1.0e-5f ) );
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
    float fade = saturate( nearestEdge * RH_VOLUME_SIZE_F * 0.60f );
    return fade * fade * ( 3.0f - 2.0f * fade );
}

// Hardware bilinear filtering inside XY slices plus manual interpolation in Z.
// Explicit LOD avoids gradient/branch restrictions in legacy ps_3_0 FXC.
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

    float4 sample0 = tex2Dlod( volumeSampler, float4( uv0, 0.0f, 0.0f ) );
    float4 sample1 = tex2Dlod( volumeSampler, float4( uv1, 0.0f, 0.0f ) );
    return lerp( sample0, sample1, zBlend );
}


// Generic flattened-atlas sampler for RH11 hierarchy levels. The caller
// supplies compile-time size constants; legacy FXC folds the arithmetic.
float4 RH_SampleAtlasN( sampler volumeSampler, float3 uvw, float volumeSize, float atlasWidth )
{
    uvw = saturate( uvw );
    float sliceWidth = 1.0f / volumeSize;
    float atlasTexel = 1.0f / atlasWidth;
    float volumeTexel = 1.0f / volumeSize;
    float innerSliceWidth = atlasTexel * ( volumeSize - 1.0f );
    float innerVolumeHeight = volumeTexel * ( volumeSize - 1.0f );
    float zPosition = uvw.z * ( volumeSize - 1.0f );
    float zSlice0 = floor( zPosition );
    float zSlice1 = min( zSlice0 + 1.0f, volumeSize - 1.0f );
    float zBlend = frac( zPosition );
    float xInSlice = atlasTexel * 0.5f + uvw.x * innerSliceWidth;
    float sampleY = volumeTexel * 0.5f + uvw.y * innerVolumeHeight;
    float2 uv0 = float2( zSlice0 * sliceWidth + xInSlice, sampleY );
    float2 uv1 = float2( zSlice1 * sliceWidth + xInSlice, sampleY );
    float4 sample0 = tex2Dlod( volumeSampler, float4( uv0, 0.0f, 0.0f ) );
    float4 sample1 = tex2Dlod( volumeSampler, float4( uv1, 0.0f, 0.0f ) );
    return lerp( sample0, sample1, zBlend );
}

float4 RH_SampleHierarchy20( sampler s, float3 uvw )
{
    return RH_SampleAtlasN( s, uvw, RH_HIERARCHY_20_SIZE_F, RH_HIERARCHY_20_ATLAS_WIDTH_F );
}

float4 RH_SampleHierarchy10( sampler s, float3 uvw )
{
    return RH_SampleAtlasN( s, uvw, RH_HIERARCHY_10_SIZE_F, RH_HIERARCHY_10_ATLAS_WIDTH_F );
}

float4 RH_SampleHierarchy5( sampler s, float3 uvw )
{
    return RH_SampleAtlasN( s, uvw, RH_HIERARCHY_5_SIZE_F, RH_HIERARCHY_5_ATLAS_WIDTH_F );
}

float4 RH_SampleShadowMip32( sampler s, float3 uvw )
{
    return RH_SampleAtlasN( s, uvw, RH_SHADOW_MIP1_SIZE_F, RH_SHADOW_MIP1_ATLAS_WIDTH_F );
}

float4 RH_SampleShadowMip16( sampler s, float3 uvw )
{
    return RH_SampleAtlasN( s, uvw, RH_SHADOW_MIP2_SIZE_F, RH_SHADOW_MIP2_ATLAS_WIDTH_F );
}

// Dedicated RH8 64^3 visibility field. It shares RH world-normalized UVW with
// the 40^3 radiance grid but uses a different flattened atlas layout.
float4 RH_SampleShadowAtlas( sampler volumeSampler, float3 uvw )
{
    uvw = saturate( uvw );

    const float sliceWidth = 1.0f / RH_SHADOW_VOLUME_SIZE_F;
    const float atlasTexel = 1.0f / RH_SHADOW_ATLAS_WIDTH_F;
    const float volumeTexel = 1.0f / RH_SHADOW_VOLUME_SIZE_F;
    const float innerSliceWidth = atlasTexel * ( RH_SHADOW_VOLUME_SIZE_F - 1.0f );
    const float innerVolumeHeight = volumeTexel * ( RH_SHADOW_VOLUME_SIZE_F - 1.0f );

    float zPosition = uvw.z * ( RH_SHADOW_VOLUME_SIZE_F - 1.0f );
    float zSlice0 = floor( zPosition );
    float zSlice1 = min( zSlice0 + 1.0f, RH_SHADOW_VOLUME_SIZE_F - 1.0f );
    float zBlend = frac( zPosition );
    float xInSlice = atlasTexel * 0.5f + uvw.x * innerSliceWidth;
    float sampleY = volumeTexel * 0.5f + uvw.y * innerVolumeHeight;
    float2 uv0 = float2( zSlice0 * sliceWidth + xInSlice, sampleY );
    float2 uv1 = float2( zSlice1 * sliceWidth + xInSlice, sampleY );
    float4 sample0 = tex2Dlod( volumeSampler, float4( uv0, 0.0f, 0.0f ) );
    float4 sample1 = tex2Dlod( volumeSampler, float4( uv1, 0.0f, 0.0f ) );
    return lerp( sample0, sample1, zBlend );
}

float RH_ShadowDistanceCells( sampler distanceSampler, float3 uvw )
{
    return RH_SampleShadowAtlas( distanceSampler, uvw ).r * RH_SHADOW_DISTANCE_MAX_CELLS;
}

float3 RH_DecodeSurfaceGuideNormal( float4 guide )
{
    return RH_SafeNormalize( guide.rgb * 2.0f - 1.0f, float3( 0.0f, 0.0f, 1.0f ) );
}

#endif // RADIANCE_HINTS_COMMON_FXC_H
