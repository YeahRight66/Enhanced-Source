#ifndef DEFERRED_RADIANCE_HINTS_H
#define DEFERRED_RADIANCE_HINTS_H

// Runtime controls. Changing these values requires no shader recompilation.
extern ConVar deferred_rh_cell_size;
extern ConVar deferred_rh_world_spread;
extern ConVar deferred_rh_injection_gain;
extern ConVar deferred_rh_bounce_count;
extern ConVar deferred_rh_bounce_gain;
extern ConVar deferred_rh_receiver_offset;
extern ConVar deferred_rh_intensity;
extern ConVar deferred_rh_rsm_edge_fade;
extern ConVar deferred_rh_min_visibility;
extern ConVar deferred_rh_validity_boost;
extern ConVar deferred_rh_saturation;
extern ConVar deferred_rh_max_radiance;
extern ConVar deferred_rh_origin_hysteresis;

// RSM-derived geometry occupancy and RH visibility.
extern ConVar deferred_rh_geometry_enable;
extern ConVar deferred_rh_geometry_inner;
extern ConVar deferred_rh_geometry_outer;
extern ConVar deferred_rh_geometry_occupancy;
extern ConVar deferred_rh_geometry_strength;
extern ConVar deferred_rh_geometry_min_transmittance;
extern ConVar deferred_rh_geometry_bias;

// Soft shadows cast by blockers in the RH occupancy field.
extern ConVar deferred_rh_soft_shadow_strength;
extern ConVar deferred_rh_soft_shadow_distance;
extern ConVar deferred_rh_soft_shadow_softness;
extern ConVar deferred_rh_soft_shadow_min_visibility;

#endif // DEFERRED_RADIANCE_HINTS_H
