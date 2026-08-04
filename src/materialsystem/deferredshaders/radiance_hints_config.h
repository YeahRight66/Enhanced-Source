#ifndef RADIANCE_HINTS_CONFIG_H
#define RADIANCE_HINTS_CONFIG_H

// Radiance Hints compile-time configuration.
//
// This file is intentionally NOT included by deferred_global_common.h or
// common_deferred_fxc.h. Changing it invalidates only RH source files.

#ifdef RH_VOLUME_SIZE
# undef RH_VOLUME_SIZE
#endif
#ifdef RH_VOLUME_SIZE_F
# undef RH_VOLUME_SIZE_F
#endif
#ifdef RH_ATLAS_WIDTH_F
# undef RH_ATLAS_WIDTH_F
#endif
#ifdef RH_ATLAS_HEIGHT_F
# undef RH_ATLAS_HEIGHT_F
#endif
#ifdef RH_ATLAS_WIDTH
# undef RH_ATLAS_WIDTH
#endif
#ifdef RH_ATLAS_HEIGHT
# undef RH_ATLAS_HEIGHT
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
#ifdef RH_GEOMETRY_TRACE_TAP_COUNT
# undef RH_GEOMETRY_TRACE_TAP_COUNT
#endif
#ifdef RH_SOFT_SHADOW_TAP_COUNT
# undef RH_SOFT_SHADOW_TAP_COUNT
#endif
#ifdef DEFRTNAME_RH_SH_R
# undef DEFRTNAME_RH_SH_R
#endif
#ifdef DEFRTNAME_RH_SH_G
# undef DEFRTNAME_RH_SH_G
#endif
#ifdef DEFRTNAME_RH_SH_B
# undef DEFRTNAME_RH_SH_B
#endif
#ifdef DEFRTNAME_RH_AUX
# undef DEFRTNAME_RH_AUX
#endif

// Cubic volume resolution. Z slices are packed horizontally in a 2D atlas.
#define RH_VOLUME_SIZE                 32
#define RH_ATLAS_WIDTH                 ( RH_VOLUME_SIZE * RH_VOLUME_SIZE )
#define RH_ATLAS_HEIGHT                RH_VOLUME_SIZE
#define RH_VOLUME_SIZE_F               ( RH_VOLUME_SIZE * 1.0f )
#define RH_ATLAS_WIDTH_F               ( RH_ATLAS_WIDTH * 1.0f )
#define RH_ATLAS_HEIGHT_F              ( RH_ATLAS_HEIGHT * 1.0f )

// Set 0 stores the first bounce. Set 1 stores one optional secondary bounce.
#define RH_SET_COUNT                   2
#define RH_CHANNEL_COUNT               4
#define RH_CHANNEL_SH_R                0
#define RH_CHANNEL_SH_G                1
#define RH_CHANNEL_SH_B                2
#define RH_CHANNEL_AUX                 3

// Fixed SM3 sampling kernels.
#define RH_RSM_SAMPLE_COUNT            8
#define RH_BOUNCE_SAMPLE_COUNT         4

// Two segment taps are enough for volume-to-volume blocking. This pass runs
// over only 32^3 cells, so it is inexpensive.
#define RH_GEOMETRY_TRACE_TAP_COUNT     2

// Full-screen soft indirect-shadow trace. Two taps are the balanced default.
// Raise to 3 only after verifying ps_3_0 register pressure on your branch.
#define RH_SOFT_SHADOW_TAP_COUNT        2

#if RH_VOLUME_SIZE < 8
# error RH_VOLUME_SIZE must be at least 8.
#endif
#if RH_RSM_SAMPLE_COUNT < 1 || RH_RSM_SAMPLE_COUNT > 8
# error RH_RSM_SAMPLE_COUNT must be in the range 1..8.
#endif
#if RH_BOUNCE_SAMPLE_COUNT < 1 || RH_BOUNCE_SAMPLE_COUNT > 4
# error RH_BOUNCE_SAMPLE_COUNT must be in the range 1..4.
#endif
#if RH_GEOMETRY_TRACE_TAP_COUNT < 1 || RH_GEOMETRY_TRACE_TAP_COUNT > 2
# error RH_GEOMETRY_TRACE_TAP_COUNT must be in the range 1..2.
#endif
#if RH_SOFT_SHADOW_TAP_COUNT < 1 || RH_SOFT_SHADOW_TAP_COUNT > 3
# error RH_SOFT_SHADOW_TAP_COUNT must be in the range 1..3.
#endif

#define DEFRTNAME_RH_SH_R              "_rt_RH_SH_R_"
#define DEFRTNAME_RH_SH_G              "_rt_RH_SH_G_"
#define DEFRTNAME_RH_SH_B              "_rt_RH_SH_B_"
#define DEFRTNAME_RH_AUX               "_rt_RH_AUX_"

#endif // RADIANCE_HINTS_CONFIG_H
