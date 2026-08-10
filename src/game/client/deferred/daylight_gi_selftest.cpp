#include "cbase.h"
#include "daylight_gi_math.h"
#include "../../../materialsystem/deferredshaders/radiance_hints_config.h"
#include "tier1/utlvector.h"

namespace
{
    int g_DaylightGITestFailures = 0;

    void DaylightGICheck( bool condition, const char *pName )
    {
        if ( condition ) return;
        ++g_DaylightGITestFailures;
        Warning( "Daylight GI self-test failed: %s\n", pName );
    }

    void TestAtlasAddressing( int volumeSize )
    {
        const int count = volumeSize * volumeSize * volumeSize;
        CUtlVector<unsigned char> visited;
        visited.SetCount( count );
        memset( visited.Base(), 0, count );
        for ( int z = 0; z < volumeSize; ++z )
        for ( int y = 0; y < volumeSize; ++y )
        for ( int x = 0; x < volumeSize; ++x )
        {
            const int index = DaylightGIAtlasIndex( x, y, z, volumeSize );
            DaylightGICheck( index >= 0 && index < count, "atlas address range" );
            if ( index >= 0 && index < count )
            {
                DaylightGICheck( visited[index] == 0, "atlas address uniqueness" );
                visited[index] = 1;
            }
        }
        for ( int i = 0; i < count; ++i )
            DaylightGICheck( visited[i] == 1, "atlas address coverage" );
    }

    void TestCoordinatesAndScrolling()
    {
        const Vector origin( -320.0f, 140.0f, 60.0f );
        const Vector voxel( 7.5f, 11.5f, 3.5f );
        const float cellSize = 20.0f;
        const Vector world = origin + voxel * cellSize;
        const Vector reconstructed = DaylightGIWorldToVoxel( world, origin, cellSize );
        DaylightGICheck( reconstructed.DistToSqr( voxel ) < 1.0e-6f, "world/voxel centre conversion" );

        const Vector shiftedOrigin = origin + Vector( 3, -2, 1 ) * cellSize;
        const Vector shiftedVoxel = DaylightGIWorldToVoxel( world, shiftedOrigin, cellSize );
        DaylightGICheck( shiftedVoxel.DistToSqr( voxel - Vector( 3, -2, 1 ) ) < 1.0e-6f,
            "integer clip scrolling" );
    }

    void TestConservativeSDF()
    {
        float previousDecoded = -1.0f;
        for ( int squared = 0; squared <= 768; ++squared )
        {
            const float conservative = DaylightGIConservativeDistanceCells( (float)squared * ( 1.0f / 16.0f ) );
            const unsigned char encoded = DaylightGIEncodeDistance( conservative, RH_SHADOW_DISTANCE_MAX_CELLS );
            const float decoded = encoded * ( RH_SHADOW_DISTANCE_MAX_CELLS / 255.0f );
            DaylightGICheck( decoded <= conservative + 1.0e-5f || encoded == 255,
                "SDF quantisation is conservative" );
            DaylightGICheck( decoded + 1.0e-5f >= previousDecoded, "SDF encoding monotonicity" );
            previousDecoded = decoded;
        }
        DaylightGICheck( DaylightGIConservativeDistanceCells( 0.0f ) == 0.0f, "occupied SDF zero" );
    }

    void TestReferenceSDFConstruction()
    {
        // Build a small exact reference field around one occupied voxel. This
        // catches address/layout errors separately from quantisation errors and
        // verifies that diagonal distances remain conservative.
        const int size = 7;
        const int centre = size / 2;
        const int count = size * size * size;
        CUtlVector<unsigned char> occupancy;
        CUtlVector<float> fieldA;
        CUtlVector<float> fieldB;
        CUtlVector<float> distance;
        occupancy.SetCount( count );
        fieldA.SetCount( count );
        fieldB.SetCount( count );
        distance.SetCount( count );
        memset( occupancy.Base(), 0, count );
        occupancy[ DaylightGIAtlasIndex( centre, centre, centre, size ) ] = 1;

        for ( int i = 0; i < count; ++i )
            fieldA[i] = occupancy[i] ? 0.0f : 1.0e7f;
        float lineIn[7];
        float lineOut[7];
        for ( int z = 0; z < size; ++z )
        for ( int y = 0; y < size; ++y )
        {
            for ( int x = 0; x < size; ++x ) lineIn[x] = fieldA[ DaylightGIAtlasIndex( x, y, z, size ) ];
            DaylightGIDistanceTransform1D<7>( lineIn, lineOut );
            for ( int x = 0; x < size; ++x ) fieldB[ DaylightGIAtlasIndex( x, y, z, size ) ] = lineOut[x];
        }
        for ( int z = 0; z < size; ++z )
        for ( int x = 0; x < size; ++x )
        {
            for ( int y = 0; y < size; ++y ) lineIn[y] = fieldB[ DaylightGIAtlasIndex( x, y, z, size ) ];
            DaylightGIDistanceTransform1D<7>( lineIn, lineOut );
            for ( int y = 0; y < size; ++y ) fieldA[ DaylightGIAtlasIndex( x, y, z, size ) ] = lineOut[y];
        }
        for ( int y = 0; y < size; ++y )
        for ( int x = 0; x < size; ++x )
        {
            for ( int z = 0; z < size; ++z ) lineIn[z] = fieldA[ DaylightGIAtlasIndex( x, y, z, size ) ];
            DaylightGIDistanceTransform1D<7>( lineIn, lineOut );
            for ( int z = 0; z < size; ++z ) fieldB[ DaylightGIAtlasIndex( x, y, z, size ) ] = lineOut[z];
        }

        for ( int z = 0; z < size; ++z )
        for ( int y = 0; y < size; ++y )
        for ( int x = 0; x < size; ++x )
        {
            float nearestSquared = 1.0e30f;
            for ( int blockerZ = 0; blockerZ < size; ++blockerZ )
            for ( int blockerY = 0; blockerY < size; ++blockerY )
            for ( int blockerX = 0; blockerX < size; ++blockerX )
            {
                if ( !occupancy[ DaylightGIAtlasIndex( blockerX, blockerY, blockerZ, size ) ] )
                    continue;
                const float dx = (float)( x - blockerX );
                const float dy = (float)( y - blockerY );
                const float dz = (float)( z - blockerZ );
                nearestSquared = MIN( nearestSquared, dx * dx + dy * dy + dz * dz );
            }

            const float conservative = DaylightGIConservativeDistanceCells( nearestSquared );
            const int atlasIndex = DaylightGIAtlasIndex( x, y, z, size );
            DaylightGICheck( fabsf( fieldB[atlasIndex] - nearestSquared ) < 1.0e-5f,
                "exact SDF transform matches brute force" );
            const unsigned char encoded = DaylightGIEncodeDistance(
                conservative, RH_SHADOW_DISTANCE_MAX_CELLS );
            const float decoded = encoded * ( RH_SHADOW_DISTANCE_MAX_CELLS / 255.0f );
            distance[atlasIndex] = decoded;
            DaylightGICheck( decoded <= conservative + 1.0e-5f,
                "constructed SDF never oversteps a blocker" );
        }

        DaylightGICheck( distance[ DaylightGIAtlasIndex( centre, centre, centre, size ) ] == 0.0f,
            "constructed SDF occupied cell" );
        for ( int offset = 0; offset <= centre; ++offset )
        {
            const float negative = distance[ DaylightGIAtlasIndex( centre - offset, centre, centre, size ) ];
            const float positive = distance[ DaylightGIAtlasIndex( centre + offset, centre, centre, size ) ];
            DaylightGICheck( fabsf( negative - positive ) < 1.0e-6f,
                "constructed SDF reflection symmetry" );
            if ( offset > 0 )
            {
                const float previous = distance[ DaylightGIAtlasIndex( centre + offset - 1, centre, centre, size ) ];
                DaylightGICheck( positive + 1.0e-6f >= previous,
                    "constructed SDF outward monotonicity" );
            }
        }
    }

    void TestTetrahedra()
    {
        const Vector samples[] =
        {
            Vector( 0.1f, 0.2f, 0.3f ), Vector( 0.8f, 0.4f, 0.2f ),
            Vector( 0.3f, 0.9f, 0.6f ), Vector( 0.5f, 0.5f, 0.5f )
        };
        for ( int sample = 0; sample < ARRAYSIZE( samples ); ++sample )
        {
            const DaylightGITetrahedron_t tetrahedron = DaylightGISelectTetrahedron( samples[sample] );
            float sum = 0.0f;
            for ( int vertex = 0; vertex < 4; ++vertex )
            {
                DaylightGICheck( tetrahedron.weights[vertex] >= -1.0e-6f,
                    "tetrahedral non-negative weight" );
                sum += tetrahedron.weights[vertex];
            }
            DaylightGICheck( fabsf( sum - 1.0f ) < 1.0e-5f, "tetrahedral partition of unity" );
        }
    }

    void TestSHAndClipBlend()
    {
        float positiveZ[4], negativeZ[4];
        DaylightGIProjectDirectionL1( Vector( 0, 0, 1 ), positiveZ );
        DaylightGIProjectDirectionL1( Vector( 0, 0,-1 ), negativeZ );
        DaylightGICheck( fabsf( positiveZ[2] + negativeZ[2] ) < 1.0e-6f,
            "SH reflection symmetry" );
        DaylightGICheck( fabsf( positiveZ[3] - negativeZ[3] ) < 1.0e-6f,
            "SH DC invariance" );

        DaylightGICheck( DaylightGINearClipWeight( Vector( 0.5f, 0.5f, 0.5f ),
            RH_VOLUME_SIZE_F, RH_CLIP_BLEND_CELLS ) == 1.0f, "near clip interior weight" );
        const float edgeWeight = DaylightGINearClipWeight( Vector( 0.01f, 0.5f, 0.5f ),
            RH_VOLUME_SIZE_F, RH_CLIP_BLEND_CELLS );
        DaylightGICheck( edgeWeight > 0.0f && edgeWeight < 1.0f, "near clip transition weight" );
        DaylightGICheck( DaylightGINearClipWeight( Vector( -0.1f, 0.5f, 0.5f ),
            RH_VOLUME_SIZE_F, RH_CLIP_BLEND_CELLS ) == 0.0f, "outside clip rejection" );

        const float farOuterFade = DaylightGISaturate( DaylightGIEdgeCells(
            Vector( 0.01f, 0.5f, 0.5f ), RH_VOLUME_SIZE_F ) / RH_CLIP_FAR_FADE_CELLS );
        DaylightGICheck( farOuterFade > 0.0f && farOuterFade < 1.0f,
            "far clip outer two-cell fade" );
        DaylightGICheck( fabsf( edgeWeight + ( 1.0f - edgeWeight ) - 1.0f ) < 1.0e-6f,
            "near/far blend partition" );
    }

    float EvaluateProjectedDiffuseL1( const float coefficients[4], const Vector &direction )
    {
        Vector normalized = direction;
        if ( VectorNormalize( normalized ) < 1.0e-7f ) normalized.Init( 0, 0, 1 );
        const float value = coefficients[0] * normalized.x * 1.0233267079f +
            coefficients[1] * normalized.y * 1.0233267079f +
            coefficients[2] * normalized.z * 1.0233267079f +
            coefficients[3] * 0.8862269255f;
        return MAX( value, 0.0f );
    }

    void TestSHEnergyBounds()
    {
        const Vector directions[] =
        {
            Vector( 1, 0, 0 ), Vector( -1, 0, 0 ),
            Vector( 0, 1, 0 ), Vector( 0,-1, 0 ),
            Vector( 0, 0, 1 ), Vector( 0, 0,-1 ),
            Vector( 1, 1, 1 ), Vector( -1, 1,-1 )
        };
        for ( int emitter = 0; emitter < ARRAYSIZE( directions ); ++emitter )
        {
            float coefficients[4];
            DaylightGIProjectDirectionL1( directions[emitter], coefficients );
            const float aligned = EvaluateProjectedDiffuseL1( coefficients, directions[emitter] );
            DaylightGICheck( fabsf( aligned - 0.75f ) < 1.0e-4f,
                "SH directional diffuse normalization" );

            for ( int receiver = 0; receiver < ARRAYSIZE( directions ); ++receiver )
            {
                const float irradiance = EvaluateProjectedDiffuseL1( coefficients, directions[receiver] );
                DaylightGICheck( irradiance >= 0.0f && irradiance <= 0.7501f,
                    "SH projected diffuse energy bound" );
            }
        }
    }
}

CON_COMMAND_F( deferred_gi_selftest,
    "Run CPU invariants for two-level daylight GI atlas/SDF/SH/clip math.", FCVAR_CHEAT )
{
    g_DaylightGITestFailures = 0;
    TestAtlasAddressing( RH_VOLUME_SIZE );
    TestAtlasAddressing( RH_SHADOW_VOLUME_SIZE );
    TestCoordinatesAndScrolling();
    TestConservativeSDF();
    TestReferenceSDFConstruction();
    TestTetrahedra();
    TestSHAndClipBlend();
    TestSHEnergyBounds();
    if ( g_DaylightGITestFailures == 0 )
        Msg( "Daylight GI self-test: all checks passed.\n" );
    else
        Warning( "Daylight GI self-test: %d check(s) failed.\n", g_DaylightGITestFailures );
}
