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
    return RH_LimitSH( sh, 1.45f );
}

float4 RH_LimitVisibilitySH( float4 sh )
{
    return RH_LimitSH( sh, 1.60f );
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
    // A projected directional impulse evaluates to ~0.796 in its own direction.
    return saturate( dot( visibilitySH, RH_ProjectDirection( direction ) ) * 1.25663706f );
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

float RH_MetaBlockerDistance( float4 meta, float maximumDistanceCells )
{
    return saturate( meta.y ) * max( maximumDistanceCells, 0.0f );
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

#endif // RADIANCE_HINTS_COMMON_FXC_H
