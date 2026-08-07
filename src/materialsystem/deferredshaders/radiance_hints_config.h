#ifndef RADIANCE_HINTS_CONFIG_H
#define RADIANCE_HINTS_CONFIG_H

// Radiance Hints 7.0 hybrid compile-time configuration.
// This header intentionally overrides legacy RH macros locally. Do not include
// it from deferred_global_common.h or common_deferred_fxc.h.

#ifdef RH_VOLUME_SIZE
# undef RH_VOLUME_SIZE
#endif
#ifdef RH_VOLUME_SIZE_F
# undef RH_VOLUME_SIZE_F
#endif
#ifdef RH_ATLAS_WIDTH
# undef RH_ATLAS_WIDTH
#endif
#ifdef RH_ATLAS_HEIGHT
# undef RH_ATLAS_HEIGHT
#endif
#ifdef RH_ATLAS_WIDTH_F
# undef RH_ATLAS_WIDTH_F
#endif
#ifdef RH_ATLAS_HEIGHT_F
# undef RH_ATLAS_HEIGHT_F
#endif
#ifdef RH_SET_COUNT
# undef RH_SET_COUNT
#endif
#ifdef RH_CHANNEL_COUNT
# undef RH_CHANNEL_COUNT
#endif
#ifdef RH_CHANNEL_SH_R
# undef RH_CHANNEL_SH_R
#endif
#ifdef RH_CHANNEL_SH_G
# undef RH_CHANNEL_SH_G
#endif
#ifdef RH_CHANNEL_SH_B
# undef RH_CHANNEL_SH_B
#endif
#ifdef RH_CHANNEL_AUX
# undef RH_CHANNEL_AUX
#endif
#ifdef RH_RSM_SAMPLE_COUNT
# undef RH_RSM_SAMPLE_COUNT
#endif
#ifdef RH_BOUNCE_SAMPLE_COUNT
# undef RH_BOUNCE_SAMPLE_COUNT
#endif

#define RH_PRESET_BALANCED 0
#define RH_PRESET_HIGH     1

#ifndef RH_QUALITY_PRESET
# define RH_QUALITY_PRESET RH_PRESET_HIGH
#endif

#if RH_QUALITY_PRESET == RH_PRESET_BALANCED
    // GTX 750/760 target.
#   define RH_VOLUME_SIZE                 32
#   define RH_RSM_RESOLUTION              512
#   define RH_RADIANCE_SAMPLE_COUNT       4
#   define RH_VISIBILITY_SAMPLE_COUNT     4
#   define RH_FILTER_NEIGHBOUR_COUNT      2
#   define RH_BOUNCE_SAMPLE_COUNT         4
#   define RH_RECEIVER_SAMPLE_COUNT       1
#   define RH_SOFT_SHADOW_TAP_COUNT       3
#   define RH_UPSAMPLE_TAP_COUNT          4
#elif RH_QUALITY_PRESET == RH_PRESET_HIGH
    // GTX 1050 target.
#   define RH_VOLUME_SIZE                 40
#   define RH_RSM_RESOLUTION              768
#   define RH_RADIANCE_SAMPLE_COUNT       16
#   define RH_VISIBILITY_SAMPLE_COUNT     8
#   define RH_FILTER_NEIGHBOUR_COUNT      2
#   define RH_BOUNCE_SAMPLE_COUNT         6
#   define RH_RECEIVER_SAMPLE_COUNT       4
#   define RH_SOFT_SHADOW_TAP_COUNT       4
#   define RH_UPSAMPLE_TAP_COUNT          4
#else
# error Invalid RH_QUALITY_PRESET.
#endif

#define RH_VOLUME_SIZE_F                   ( RH_VOLUME_SIZE * 1.0f )
#define RH_ATLAS_WIDTH                     ( RH_VOLUME_SIZE * RH_VOLUME_SIZE )
#define RH_ATLAS_HEIGHT                    RH_VOLUME_SIZE
#define RH_ATLAS_WIDTH_F                   ( RH_ATLAS_WIDTH * 1.0f )
#define RH_ATLAS_HEIGHT_F                  ( RH_ATLAS_HEIGHT * 1.0f )
#define RH_RSM_RESOLUTION_F                ( RH_RSM_RESOLUTION * 1.0f )

// Three four-MRT sets are used: set 0 is the final first-bounce field,
// set 1 stores freshly generated surface bounce, and set 2 stores its
// geometry-aware transport result. Metadata from set 0 remains authoritative.
#define RH_SET_COUNT                       3
#define RH_CHANNEL_COUNT                   4
#define RH_CHANNEL_SH_R                    0
#define RH_CHANNEL_SH_G                    1
#define RH_CHANNEL_SH_B                    2
#define RH_CHANNEL_META                    3
#define RH_CHANNEL_AUX                     RH_CHANNEL_META

#define RH_GEOMETRY_TRACE_TAP_COUNT        2

#if RH_VOLUME_SIZE < 16 || RH_VOLUME_SIZE > 48
# error RH_VOLUME_SIZE must be in the range 16..48.
#endif
#if RH_RADIANCE_SAMPLE_COUNT < 1 || RH_RADIANCE_SAMPLE_COUNT > 16
# error RH_RADIANCE_SAMPLE_COUNT must be in the range 1..16.
#endif
#if RH_VISIBILITY_SAMPLE_COUNT < 1 || RH_VISIBILITY_SAMPLE_COUNT > 8
# error RH_VISIBILITY_SAMPLE_COUNT must be in the range 1..8.
#endif
#if RH_FILTER_NEIGHBOUR_COUNT != 2
# error RH_FILTER_NEIGHBOUR_COUNT must be 2 for separable X/Y/Z reconstruction.
#endif
#if RH_BOUNCE_SAMPLE_COUNT < 1 || RH_BOUNCE_SAMPLE_COUNT > 6
# error RH_BOUNCE_SAMPLE_COUNT must be in the range 1..6.
#endif
#if RH_RECEIVER_SAMPLE_COUNT < 1 || RH_RECEIVER_SAMPLE_COUNT > 4
# error RH_RECEIVER_SAMPLE_COUNT must be in the range 1..4.
#endif
#if RH_SOFT_SHADOW_TAP_COUNT < 2 || RH_SOFT_SHADOW_TAP_COUNT > 4
# error RH_SOFT_SHADOW_TAP_COUNT must be in the range 2..4.
#endif
#if RH_UPSAMPLE_TAP_COUNT != 4
# error RH_UPSAMPLE_TAP_COUNT must be 4.
#endif

// Remove legacy target-name definitions locally.
#ifdef DEFRTNAME_RH_SH_R
# undef DEFRTNAME_RH_SH_R
#endif
#ifdef DEFRTNAME_RH_SH_G
# undef DEFRTNAME_RH_SH_G
#endif
#ifdef DEFRTNAME_RH_SH_B
# undef DEFRTNAME_RH_SH_B
#endif
#ifdef DEFRTNAME_RH_META
# undef DEFRTNAME_RH_META
#endif
#ifdef DEFRTNAME_RH_VISIBILITY
# undef DEFRTNAME_RH_VISIBILITY
#endif
#ifdef DEFRTNAME_RH_AUX
# undef DEFRTNAME_RH_AUX
#endif
#ifdef DEFRTNAME_RH_RSM_DEPTH
# undef DEFRTNAME_RH_RSM_DEPTH
#endif
#ifdef DEFRTNAME_RH_RSM_COLOR
# undef DEFRTNAME_RH_RSM_COLOR
#endif
#ifdef DEFRTNAME_RH_RSM_FLUX
# undef DEFRTNAME_RH_RSM_FLUX
#endif
#ifdef DEFRTNAME_RH_RSM_NORMAL
# undef DEFRTNAME_RH_RSM_NORMAL
#endif
#ifdef DEFRTNAME_RH_RSM_ALBEDO
# undef DEFRTNAME_RH_RSM_ALBEDO
#endif
#ifdef DEFRTNAME_RH_SURFACE_ALBEDO
# undef DEFRTNAME_RH_SURFACE_ALBEDO
#endif
#ifdef DEFRTNAME_RH_SURFACE_NORMAL
# undef DEFRTNAME_RH_SURFACE_NORMAL
#endif
#ifdef DEFRTNAME_RH_GEOMETRY_DISTANCE
# undef DEFRTNAME_RH_GEOMETRY_DISTANCE
#endif
#ifdef DEFRTNAME_RH_INDIRECT_HALF
# undef DEFRTNAME_RH_INDIRECT_HALF
#endif
#ifdef DEFRTNAME_RH_GEOMETRY
# undef DEFRTNAME_RH_GEOMETRY
#endif

#define DEFRTNAME_RH_SH_R                  "_rt_RH_SH_R_"
#define DEFRTNAME_RH_SH_G                  "_rt_RH_SH_G_"
#define DEFRTNAME_RH_SH_B                  "_rt_RH_SH_B_"
#define DEFRTNAME_RH_META                  "_rt_RH_Meta_"
#define DEFRTNAME_RH_AUX                   DEFRTNAME_RH_META
#define DEFRTNAME_RH_VISIBILITY            "_rt_RH_Visibility"

#define DEFRTNAME_RH_RSM_DEPTH             "_rt_RH_RSM_Depth"
#define DEFRTNAME_RH_RSM_COLOR             "_rt_RH_RSM_ColorDepth"
#define DEFRTNAME_RH_RSM_FLUX              "_rt_RH_RSM_Flux"
#define DEFRTNAME_RH_RSM_NORMAL            "_rt_RH_RSM_Normal"
#define DEFRTNAME_RH_RSM_ALBEDO            "_rt_RH_RSM_Albedo"
#define DEFRTNAME_RH_SURFACE_ALBEDO        "_rt_RH_SurfaceAlbedo"
#define DEFRTNAME_RH_SURFACE_NORMAL        "_rt_RH_SurfaceNormal"
#define DEFRTNAME_RH_GEOMETRY_DISTANCE     "_rt_RH_GeometryDistance"
#define DEFRTNAME_RH_INDIRECT_HALF         "_rt_RH_IndirectHalf"
#define DEFRTNAME_RH_GEOMETRY              "_rt_RH_Geometry"

#endif // RADIANCE_HINTS_CONFIG_H

