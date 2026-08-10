#ifndef DAYLIGHT_GI_MATH_H
#define DAYLIGHT_GI_MATH_H

#include "mathlib/vector.h"
#include <math.h>

inline int DaylightGIAtlasIndex( int x, int y, int z, int volumeSize )
{
    return y * volumeSize * volumeSize + z * volumeSize + x;
}

inline Vector DaylightGIWorldToVoxel( const Vector &worldPosition,
    const Vector &volumeOrigin, float cellSize )
{
    const float inverseCell = 1.0f / MAX( cellSize, 1.0e-6f );
    return ( worldPosition - volumeOrigin ) * inverseCell;
}

inline float DaylightGIConservativeDistanceCells( float squaredCenterDistance )
{
    // Occupancy samples represent cubes, not points. The half diagonal makes
    // this a lower bound to the occupied voxel surface in every direction.
    return MAX( sqrtf( MAX( squaredCenterDistance, 0.0f ) ) - 0.8660254038f, 0.0f );
}

inline unsigned char DaylightGIEncodeDistance( float distanceCells, float maximumCells )
{
    const float clampedDistance = MIN( MAX( distanceCells, 0.0f ), MAX( maximumCells, 1.0e-6f ) );
    return (unsigned char)MIN( MAX( (int)floorf(
        clampedDistance * ( 255.0f / MAX( maximumCells, 1.0e-6f ) ) ), 0 ), 255 );
}

template <int LINE_SIZE>
inline void DaylightGIDistanceTransform1D( const float *f, float *d )
{
    // Felzenszwalb/Huttenlocher lower-envelope transform for squared Euclidean
    // distance. Inputs must be zero at occupied samples and a large finite value
    // elsewhere; keeping the sentinel finite also handles completely empty lines.
    int vertices[ LINE_SIZE ];
    float boundaries[ LINE_SIZE + 1 ];
    int envelope = 0;
    vertices[0] = 0;
    boundaries[0] = -1.0e20f;
    boundaries[1] =  1.0e20f;

    for ( int sample = 1; sample < LINE_SIZE; ++sample )
    {
        float separation = 0.0f;
        for ( ;; )
        {
            const int vertex = vertices[envelope];
            separation = ( ( f[sample] + (float)( sample * sample ) ) -
                ( f[vertex] + (float)( vertex * vertex ) ) ) /
                ( 2.0f * (float)( sample - vertex ) );
            if ( separation > boundaries[envelope] || envelope == 0 )
                break;
            --envelope;
        }
        if ( separation <= boundaries[envelope] )
            separation = boundaries[envelope] + 1.0e-4f;
        ++envelope;
        vertices[envelope] = sample;
        boundaries[envelope] = separation;
        boundaries[envelope + 1] = 1.0e20f;
    }

    envelope = 0;
    for ( int sample = 0; sample < LINE_SIZE; ++sample )
    {
        while ( boundaries[envelope + 1] < (float)sample )
            ++envelope;
        const float delta = (float)( sample - vertices[envelope] );
        d[sample] = delta * delta + f[ vertices[envelope] ];
    }
}

inline float DaylightGIEdgeCells( const Vector &uvw, float volumeSize )
{
    const float nearest = MIN( MIN( MIN( uvw.x, uvw.y ), uvw.z ),
        MIN( MIN( 1.0f - uvw.x, 1.0f - uvw.y ), 1.0f - uvw.z ) );
    return MAX( nearest * volumeSize, 0.0f );
}

inline float DaylightGISaturate( float value )
{
    return MIN( MAX( value, 0.0f ), 1.0f );
}

inline float DaylightGINearClipWeight( const Vector &uvw, float volumeSize, float blendCells )
{
    const bool inside = uvw.x >= 0.0f && uvw.x <= 1.0f &&
        uvw.y >= 0.0f && uvw.y <= 1.0f && uvw.z >= 0.0f && uvw.z <= 1.0f;
    return inside ? DaylightGISaturate( DaylightGIEdgeCells( uvw, volumeSize ) /
        MAX( blendCells, 1.0f ) ) : 0.0f;
}

struct DaylightGITetrahedron_t
{
    Vector corners[4];
    float weights[4];
};

inline DaylightGITetrahedron_t DaylightGISelectTetrahedron( const Vector &fraction )
{
    DaylightGITetrahedron_t result;
    result.corners[0].Init( 0.0f, 0.0f, 0.0f );
    result.corners[3].Init( 1.0f, 1.0f, 1.0f );
    if ( fraction.x >= fraction.y )
    {
        if ( fraction.y >= fraction.z )
        {
            result.corners[1].Init( 1, 0, 0 ); result.corners[2].Init( 1, 1, 0 );
            result.weights[0] = 1.0f - fraction.x; result.weights[1] = fraction.x - fraction.y;
            result.weights[2] = fraction.y - fraction.z; result.weights[3] = fraction.z;
        }
        else if ( fraction.x >= fraction.z )
        {
            result.corners[1].Init( 1, 0, 0 ); result.corners[2].Init( 1, 0, 1 );
            result.weights[0] = 1.0f - fraction.x; result.weights[1] = fraction.x - fraction.z;
            result.weights[2] = fraction.z - fraction.y; result.weights[3] = fraction.y;
        }
        else
        {
            result.corners[1].Init( 0, 0, 1 ); result.corners[2].Init( 1, 0, 1 );
            result.weights[0] = 1.0f - fraction.z; result.weights[1] = fraction.z - fraction.x;
            result.weights[2] = fraction.x - fraction.y; result.weights[3] = fraction.y;
        }
    }
    else
    {
        if ( fraction.x >= fraction.z )
        {
            result.corners[1].Init( 0, 1, 0 ); result.corners[2].Init( 1, 1, 0 );
            result.weights[0] = 1.0f - fraction.y; result.weights[1] = fraction.y - fraction.x;
            result.weights[2] = fraction.x - fraction.z; result.weights[3] = fraction.z;
        }
        else if ( fraction.y >= fraction.z )
        {
            result.corners[1].Init( 0, 1, 0 ); result.corners[2].Init( 0, 1, 1 );
            result.weights[0] = 1.0f - fraction.y; result.weights[1] = fraction.y - fraction.z;
            result.weights[2] = fraction.z - fraction.x; result.weights[3] = fraction.x;
        }
        else
        {
            result.corners[1].Init( 0, 0, 1 ); result.corners[2].Init( 0, 1, 1 );
            result.weights[0] = 1.0f - fraction.z; result.weights[1] = fraction.z - fraction.y;
            result.weights[2] = fraction.y - fraction.x; result.weights[3] = fraction.x;
        }
    }
    return result;
}

inline void DaylightGIProjectDirectionL1( const Vector &direction, float coefficients[4] )
{
    Vector normalized = direction;
    if ( VectorNormalize( normalized ) < 1.0e-7f ) normalized.Init( 0, 0, 1 );
    coefficients[0] = normalized.x * 0.4886025119f;
    coefficients[1] = normalized.y * 0.4886025119f;
    coefficients[2] = normalized.z * 0.4886025119f;
    coefficients[3] = 0.2820947918f;
}

#endif
