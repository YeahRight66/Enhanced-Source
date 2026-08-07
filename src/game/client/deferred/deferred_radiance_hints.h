#ifndef DEFERRED_RADIANCE_HINTS_H
#define DEFERRED_RADIANCE_HINTS_H

// Runtime controls. None of these require shader recompilation.
extern ConVar deferred_rh_cell_size;
extern ConVar deferred_rh_gather_radius_cells;
extern ConVar deferred_rh_world_spread;
extern ConVar deferred_rh_injection_gain;
extern ConVar deferred_rh_bounce_count;
extern ConVar deferred_rh_bounce_gain;
extern ConVar deferred_rh_receiver_offset;
extern ConVar deferred_rh_intensity;
extern ConVar deferred_rh_rsm_edge_fade;
extern ConVar deferred_rh_saturation;
extern ConVar deferred_rh_max_radiance;
extern ConVar deferred_rh_origin_hysteresis;
extern ConVar deferred_rh_rsm_padding;
extern ConVar deferred_rh_back_rsm_enable;

// Directional blocker field.
extern ConVar deferred_rh_visibility_inner;
extern ConVar deferred_rh_visibility_outer;
extern ConVar deferred_rh_visibility_strength;
extern ConVar deferred_rh_visibility_decay;
extern ConVar deferred_rh_geometry_strength;
extern ConVar deferred_rh_geometry_min_transmittance;
extern ConVar deferred_rh_geometry_bias;
extern ConVar deferred_rh_cpu_geometry_enable;
extern ConVar deferred_rh_dynamic_model_blockers;
extern ConVar deferred_rh_geometry_hull_scale;

// Low-frequency indirect shadows.
extern ConVar deferred_rh_soft_shadow_strength;
extern ConVar deferred_rh_soft_shadow_distance;
extern ConVar deferred_rh_soft_shadow_softness;
extern ConVar deferred_rh_soft_shadow_min_visibility;

// Half-resolution bilateral reconstruction.
extern ConVar deferred_rh_upsample_depth_scale;
extern ConVar deferred_rh_upsample_normal_power;

extern ConVar deferred_rh_visibility_dilation;
extern ConVar deferred_rh_visibility_normal_weight;
extern ConVar deferred_rh_filter_strength;
extern ConVar deferred_rh_filter_fill_boost;
extern ConVar deferred_rh_filter_energy_scale;
extern ConVar deferred_rh_filter_direction_preserve;
extern ConVar deferred_rh_filter_radius;
extern ConVar deferred_rh_bounce_trace_width;
extern ConVar deferred_rh_bounce_min_confidence;
extern ConVar deferred_rh_diffusion_gain;
extern ConVar deferred_rh_diffusion_radius;
extern ConVar deferred_rh_diffusion_min_confidence;
extern ConVar deferred_rh_receiver_radius;
extern ConVar deferred_rh_reconstruction_confidence_floor;
extern ConVar deferred_rh_shadow_isotropic_blend;


extern ConVar deferred_rh_sky_enable;
extern ConVar deferred_rh_sky_intensity;
extern ConVar deferred_rh_sky_upper_scale;
extern ConVar deferred_rh_sky_lower_scale;
extern ConVar deferred_rh_sky_occlusion;
extern ConVar deferred_rh_sky_trace_distance;
extern ConVar deferred_rh_sky_replace_global_ambient;
extern ConVar deferred_rh_surface_bounce_gain;
extern ConVar deferred_rh_surface_radius;
extern ConVar deferred_rh_surface_min_coverage;
extern ConVar deferred_rh_shadow_extinction;
extern ConVar deferred_rh_shadow_start_cells;
extern ConVar deferred_rh_shadow_receiver_clearance;
extern ConVar deferred_rh_shadow_contact_strength;
extern ConVar deferred_rh_shadow_far_strength;

// RH8 high-resolution SDF visibility.
extern ConVar deferred_rh_shadow_sdf_enable;
extern ConVar deferred_rh_shadow_sdf_steps;
extern ConVar deferred_rh_shadow_sdf_second_steps;
extern ConVar deferred_rh_shadow_sdf_step_scale;
extern ConVar deferred_rh_shadow_sdf_min_step;
extern ConVar deferred_rh_shadow_sdf_max_step;
extern ConVar deferred_rh_shadow_sdf_softness;
extern ConVar deferred_rh_shadow_second_strength;

// RH9 surface-guide/classification/relocation and adaptive sampling.
extern ConVar deferred_rh_surface_cache_enable;
extern ConVar deferred_rh_surface_cache_blend;
extern ConVar deferred_rh_surface_fallback_albedo;
extern ConVar deferred_rh_cell_relocation;
extern ConVar deferred_rh_adaptive_gather;
extern ConVar deferred_rh_adaptive_gather_near;
extern ConVar deferred_rh_adaptive_gather_far;
extern ConVar deferred_rh_adaptive_diffusion;
extern ConVar deferred_rh_adaptive_diffusion_near;
extern ConVar deferred_rh_adaptive_diffusion_far;

extern ConVar deferred_rh_debug_mode;

// Compatibility aliases retained for old configs; RH7 no longer uses them
// as scalar occupancy/validity controls.
extern ConVar deferred_rh_min_visibility;
extern ConVar deferred_rh_validity_boost;
extern ConVar deferred_rh_geometry_enable;
extern ConVar deferred_rh_geometry_inner;
extern ConVar deferred_rh_geometry_outer;
extern ConVar deferred_rh_geometry_occupancy;

#endif // DEFERRED_RADIANCE_HINTS_H
