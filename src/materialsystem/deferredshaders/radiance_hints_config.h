#ifndef RADIANCE_HINTS_CONFIG_H
#define RADIANCE_HINTS_CONFIG_H

// Radiance Hints 4.0 compile-time configuration.
// This header is intentionally isolated from deferred_global_common.h and
// common_deferred_fxc.h so unrelated deferred shaders are not invalidated.

// Older Enhanced Source branches placed RH macros in deferred_global_common.h.
// Override them locally without modifying that shared header or rebuilding every
// unrelated deferred shader.
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
#define RH_QUALITY_PRESET RH_PRESET_HIGH
#endif

#if RH_QUALITY_PRESET == RH_PRESET_BALANCED
    // GTX 750/760 target.
    #define RH_VOLUME_SIZE                 32
    #define RH_RSM_RESOLUTION              512
    #define RH_RADIANCE_SAMPLE_COUNT       4
    #define RH_VISIBILITY_SAMPLE_COUNT     4
    #define RH_BOUNCE_SAMPLE_COUNT         4
    #define RH_SOFT_SHADOW_TAP_COUNT       3
#elif RH_QUALITY_PRESET == RH_PRESET_HIGH
    // GTX 1050 target.
    #define RH_VOLUME_SIZE                 40
    #define RH_RSM_RESOLUTION              768
    #define RH_RADIANCE_SAMPLE_COUNT       6
    #define RH_VISIBILITY_SAMPLE_COUNT     4
    #define RH_BOUNCE_SAMPLE_COUNT         4
    #define RH_SOFT_SHADOW_TAP_COUNT       4
#else
    #error Invalid RH_QUALITY_PRESET.
#endif

#define RH_VOLUME_SIZE_F                   ( RH_VOLUME_SIZE * 1.0f )
#define RH_ATLAS_WIDTH                     ( RH_VOLUME_SIZE * RH_VOLUME_SIZE )
#define RH_ATLAS_HEIGHT                    RH_VOLUME_SIZE
#define RH_ATLAS_WIDTH_F                   ( RH_ATLAS_WIDTH * 1.0f )
#define RH_ATLAS_HEIGHT_F                  ( RH_ATLAS_HEIGHT * 1.0f )
#define RH_RSM_RESOLUTION_F                ( RH_RSM_RESOLUTION * 1.0f )

// Set 0 = deterministic first bounce, set 1 = one optional secondary bounce.
#define RH_SET_COUNT                       2
#define RH_CHANNEL_COUNT                   4
#define RH_CHANNEL_SH_R                    0
#define RH_CHANNEL_SH_G                    1
#define RH_CHANNEL_SH_B                    2
#define RH_CHANNEL_VISIBILITY              3
#define RH_CHANNEL_AUX                     RH_CHANNEL_VISIBILITY

#define RH_GEOMETRY_TRACE_TAP_COUNT        2

#if RH_VOLUME_SIZE < 16 || RH_VOLUME_SIZE > 48
    #error RH_VOLUME_SIZE must be in the range 16..48.
#endif
#if RH_RADIANCE_SAMPLE_COUNT < 1 || RH_RADIANCE_SAMPLE_COUNT > 8
    #error RH_RADIANCE_SAMPLE_COUNT must be in the range 1..8.
#endif
#if RH_VISIBILITY_SAMPLE_COUNT < 1 || RH_VISIBILITY_SAMPLE_COUNT > 6
    #error RH_VISIBILITY_SAMPLE_COUNT must be in the range 1..6.
#endif
#if RH_BOUNCE_SAMPLE_COUNT < 1 || RH_BOUNCE_SAMPLE_COUNT > 4
    #error RH_BOUNCE_SAMPLE_COUNT must be in the range 1..4.
#endif
#if RH_SOFT_SHADOW_TAP_COUNT < 2 || RH_SOFT_SHADOW_TAP_COUNT > 4
    #error RH_SOFT_SHADOW_TAP_COUNT must be in the range 2..4.
#endif

#define DEFRTNAME_RH_SH_R                  "_rt_RH_SH_R_"
#define DEFRTNAME_RH_SH_G                  "_rt_RH_SH_G_"
#define DEFRTNAME_RH_SH_B                  "_rt_RH_SH_B_"
#define DEFRTNAME_RH_VISIBILITY            "_rt_RH_Visibility_"
#define DEFRTNAME_RH_AUX                   DEFRTNAME_RH_VISIBILITY

#define DEFRTNAME_RH_RSM_DEPTH             "_rt_RH_RSM_Depth"
#define DEFRTNAME_RH_RSM_COLOR             "_rt_RH_RSM_ColorDepth"
#define DEFRTNAME_RH_RSM_FLUX              "_rt_RH_RSM_Flux"
#define DEFRTNAME_RH_RSM_NORMAL            "_rt_RH_RSM_Normal"
#define DEFRTNAME_RH_INDIRECT_HALF         "_rt_RH_IndirectHalf"

#endif // RADIANCE_HINTS_CONFIG_H

