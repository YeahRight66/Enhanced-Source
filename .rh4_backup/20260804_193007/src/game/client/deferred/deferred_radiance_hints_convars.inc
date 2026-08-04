// Included exactly once by deferred_client_common.cpp.

ConVar deferred_rh_cell_size(
    "deferred_rh_cell_size", "48", FCVAR_ARCHIVE,
    "World-space Radiance Hints cell size." );

ConVar deferred_rh_world_spread(
    "deferred_rh_world_spread", "320", FCVAR_ARCHIVE,
    "Maximum RSM/RH transfer distance in world units." );

ConVar deferred_rh_injection_gain(
    "deferred_rh_injection_gain", "1.0", FCVAR_ARCHIVE,
    "First-bounce RSM injection gain." );

ConVar deferred_rh_bounce_count(
    "deferred_rh_bounce_count", "1", FCVAR_ARCHIVE,
    "Secondary RH bounces; the SM3 path supports 0 or 1." );

ConVar deferred_rh_bounce_gain(
    "deferred_rh_bounce_gain", "0.35", FCVAR_ARCHIVE,
    "Secondary-bounce transport gain." );

ConVar deferred_rh_receiver_offset(
    "deferred_rh_receiver_offset", "18", FCVAR_ARCHIVE,
    "Normal offset used while reconstructing indirect light." );

ConVar deferred_rh_intensity(
    "deferred_rh_intensity", "1.0", FCVAR_ARCHIVE,
    "Final Radiance Hints indirect-light intensity." );

ConVar deferred_rh_rsm_edge_fade(
    "deferred_rh_rsm_edge_fade", "0.025", FCVAR_ARCHIVE,
    "Smooth local-UV fade at the RSM footprint edge." );

ConVar deferred_rh_min_visibility(
    "deferred_rh_min_visibility", "0.02", FCVAR_ARCHIVE,
    "Small legacy visibility floor for the optional secondary bounce." );

ConVar deferred_rh_validity_boost(
    "deferred_rh_validity_boost", "3.0", FCVAR_ARCHIVE,
    "Boosts sparse but valid RH cells without filling empty cells." );

ConVar deferred_rh_saturation(
    "deferred_rh_saturation", "1.05", FCVAR_ARCHIVE,
    "Indirect-light colour saturation; 1 is neutral." );

ConVar deferred_rh_max_radiance(
    "deferred_rh_max_radiance", "16.0", FCVAR_ARCHIVE,
    "FP16/firefly safety clamp for RH transport and reconstruction." );

ConVar deferred_rh_origin_hysteresis(
    "deferred_rh_origin_hysteresis", "1.5", FCVAR_ARCHIVE,
    "Volume-origin movement threshold in cell units." );

ConVar deferred_rh_geometry_enable(
    "deferred_rh_geometry_enable", "1", FCVAR_ARCHIVE,
    "Build RSM-derived RH occupancy and use it for transport visibility." );

ConVar deferred_rh_geometry_inner(
    "deferred_rh_geometry_inner", "0.18", FCVAR_ARCHIVE,
    "Fully occupied radius around an RSM surface, in RH cell units." );

ConVar deferred_rh_geometry_outer(
    "deferred_rh_geometry_outer", "0.95", FCVAR_ARCHIVE,
    "Soft occupancy falloff radius around an RSM surface, in RH cell units." );

ConVar deferred_rh_geometry_occupancy(
    "deferred_rh_geometry_occupancy", "1.0", FCVAR_ARCHIVE,
    "Occupancy amplitude written into RH Aux.w." );

ConVar deferred_rh_geometry_strength(
    "deferred_rh_geometry_strength", "0.90", FCVAR_ARCHIVE,
    "Strength of geometry blocking during RH secondary transport." );

ConVar deferred_rh_geometry_min_transmittance(
    "deferred_rh_geometry_min_transmittance", "0.08", FCVAR_ARCHIVE,
    "Minimum secondary-bounce transmittance through an occupied RH segment." );

ConVar deferred_rh_geometry_bias(
    "deferred_rh_geometry_bias", "0.10", FCVAR_ARCHIVE,
    "Occupancy threshold used to suppress self-blocking and weak noise." );

ConVar deferred_rh_soft_shadow_strength(
    "deferred_rh_soft_shadow_strength", "0.80", FCVAR_ARCHIVE,
    "Strength of RH soft indirect shadows; 0 disables the screen trace." );

ConVar deferred_rh_soft_shadow_distance(
    "deferred_rh_soft_shadow_distance", "4.0", FCVAR_ARCHIVE,
    "Soft indirect-shadow trace length in RH cell units." );

ConVar deferred_rh_soft_shadow_softness(
    "deferred_rh_soft_shadow_softness", "0.65", FCVAR_ARCHIVE,
    "Penumbra softness; 0 preserves thin blockers, 1 averages trace taps." );

ConVar deferred_rh_soft_shadow_min_visibility(
    "deferred_rh_soft_shadow_min_visibility", "0.18", FCVAR_ARCHIVE,
    "Minimum visibility under RH soft indirect shadows." );
