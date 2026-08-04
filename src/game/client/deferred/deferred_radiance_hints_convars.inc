// Included exactly once by deferred_client_common.cpp.

ConVar deferred_rh_cell_size(
    "deferred_rh_cell_size", "44", FCVAR_ARCHIVE,
    "World-space RH cell size. Use 48 for the 32^3 balanced preset." );

ConVar deferred_rh_world_spread(
    "deferred_rh_world_spread", "240", FCVAR_ARCHIVE,
    "World-space radius used by deterministic RSM radiance gathers." );

ConVar deferred_rh_injection_gain(
    "deferred_rh_injection_gain", "1.0", FCVAR_ARCHIVE,
    "First-bounce RSM injection gain." );

ConVar deferred_rh_bounce_count(
    "deferred_rh_bounce_count", "1", FCVAR_ARCHIVE,
    "Secondary RH bounces; the production SM3 path supports 0 or 1." );

ConVar deferred_rh_bounce_gain(
    "deferred_rh_bounce_gain", "0.30", FCVAR_ARCHIVE,
    "Energy-bounded secondary-bounce gain." );

ConVar deferred_rh_receiver_offset(
    "deferred_rh_receiver_offset", "16", FCVAR_ARCHIVE,
    "Normal offset used while reconstructing indirect light." );

ConVar deferred_rh_intensity(
    "deferred_rh_intensity", "1.0", FCVAR_ARCHIVE,
    "Final RH indirect-light intensity." );

ConVar deferred_rh_rsm_edge_fade(
    "deferred_rh_rsm_edge_fade", "0.02", FCVAR_ARCHIVE,
    "Smooth UV fade at the dedicated RSM boundary." );

ConVar deferred_rh_saturation(
    "deferred_rh_saturation", "1.0", FCVAR_ARCHIVE,
    "Final indirect-light colour saturation; 1 is neutral." );

ConVar deferred_rh_max_radiance(
    "deferred_rh_max_radiance", "20.0", FCVAR_ARCHIVE,
    "FP16/firefly safety clamp for RH transport and reconstruction." );

ConVar deferred_rh_origin_hysteresis(
    "deferred_rh_origin_hysteresis", "0.5", FCVAR_ARCHIVE,
    "Camera movement dead-zone in RH cell units. Rotation never moves RH." );

ConVar deferred_rh_rsm_padding(
    "deferred_rh_rsm_padding", "2.0", FCVAR_ARCHIVE,
    "Dedicated RSM padding around the RH cube, in cell units." );

ConVar deferred_rh_visibility_inner(
    "deferred_rh_visibility_inner", "0.12", FCVAR_ARCHIVE,
    "Fully occupied radius around an RSM surface, in RH cell units." );

ConVar deferred_rh_visibility_outer(
    "deferred_rh_visibility_outer", "1.10", FCVAR_ARCHIVE,
    "Directional blocker falloff radius, in RH cell units." );

ConVar deferred_rh_visibility_strength(
    "deferred_rh_visibility_strength", "1.0", FCVAR_ARCHIVE,
    "Amplitude of the directional blocker field." );

ConVar deferred_rh_visibility_decay(
    "deferred_rh_visibility_decay", "0.35", FCVAR_ARCHIVE,
    "Distance decay used by the blocker injection pass." );

ConVar deferred_rh_geometry_strength(
    "deferred_rh_geometry_strength", "1.0", FCVAR_ARCHIVE,
    "Directional visibility strength during secondary transport." );

ConVar deferred_rh_geometry_min_transmittance(
    "deferred_rh_geometry_min_transmittance", "0.08", FCVAR_ARCHIVE,
    "Minimum secondary-bounce transmittance through blockers." );

ConVar deferred_rh_geometry_bias(
    "deferred_rh_geometry_bias", "0.05", FCVAR_ARCHIVE,
    "Visibility-field bias used to suppress receiver self-blocking." );

ConVar deferred_rh_soft_shadow_strength(
    "deferred_rh_soft_shadow_strength", "0.75", FCVAR_ARCHIVE,
    "Strength of low-frequency RH indirect shadows." );

ConVar deferred_rh_soft_shadow_distance(
    "deferred_rh_soft_shadow_distance", "4.5", FCVAR_ARCHIVE,
    "Indirect-shadow cone length in RH cells." );

ConVar deferred_rh_soft_shadow_softness(
    "deferred_rh_soft_shadow_softness", "0.75", FCVAR_ARCHIVE,
    "Cone widening and blocker averaging for indirect penumbrae." );

ConVar deferred_rh_soft_shadow_min_visibility(
    "deferred_rh_soft_shadow_min_visibility", "0.20", FCVAR_ARCHIVE,
    "Minimum indirect visibility under RH soft shadows." );

ConVar deferred_rh_upsample_depth_scale(
    "deferred_rh_upsample_depth_scale", "0.025", FCVAR_ARCHIVE,
    "Depth rejection scale for half-resolution bilateral upsampling." );

ConVar deferred_rh_upsample_normal_power(
    "deferred_rh_upsample_normal_power", "16.0", FCVAR_ARCHIVE,
    "Normal similarity exponent for RH bilateral upsampling." );

ConVar deferred_rh_debug_mode(
    "deferred_rh_debug_mode", "0", FCVAR_NONE,
    "RH debug mode. 0=off, 1=volume bounds/RSM status." );

// Legacy compatibility controls.
ConVar deferred_rh_min_visibility(
    "deferred_rh_min_visibility", "0.02", FCVAR_ARCHIVE,
    "Legacy compatibility setting; retained for existing configs." );
ConVar deferred_rh_validity_boost(
    "deferred_rh_validity_boost", "1.0", FCVAR_ARCHIVE,
    "Legacy compatibility setting; RH 4.0 derives confidence from SH energy." );
ConVar deferred_rh_geometry_enable(
    "deferred_rh_geometry_enable", "1", FCVAR_ARCHIVE,
    "Enables directional RH visibility and soft indirect shadows." );
ConVar deferred_rh_geometry_inner(
    "deferred_rh_geometry_inner", "0.12", FCVAR_ARCHIVE,
    "Legacy alias for deferred_rh_visibility_inner." );
ConVar deferred_rh_geometry_outer(
    "deferred_rh_geometry_outer", "1.10", FCVAR_ARCHIVE,
    "Legacy alias for deferred_rh_visibility_outer." );
ConVar deferred_rh_geometry_occupancy(
    "deferred_rh_geometry_occupancy", "1.0", FCVAR_ARCHIVE,
    "Legacy alias for deferred_rh_visibility_strength." );
