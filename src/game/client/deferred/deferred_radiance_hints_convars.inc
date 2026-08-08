// Included exactly once by deferred_client_common.cpp.

ConVar deferred_rh_cell_size(
    "deferred_rh_cell_size", "20", FCVAR_ARCHIVE,
    "World-space RH cell size. RH9 High uses a dense 40^3 radiance volume (800 units at the default)." );

ConVar deferred_rh_gather_radius_cells(
    "deferred_rh_gather_radius_cells", "5.5", FCVAR_ARCHIVE,
    "RSM radiance gather radius measured in RH cells; independent of cell size." );

ConVar deferred_rh_world_spread(
    "deferred_rh_world_spread", "0", FCVAR_ARCHIVE,
    "Legacy override for gather radius in world units. Zero uses deferred_rh_gather_radius_cells." );

ConVar deferred_rh_injection_gain(
    "deferred_rh_injection_gain", "1.0", FCVAR_ARCHIVE,
    "First-bounce RSM injection gain." );

ConVar deferred_rh_bounce_count(
    "deferred_rh_bounce_count", "1", FCVAR_ARCHIVE,
    "Secondary RH bounces; the production SM3 path supports 0 or 1." );

ConVar deferred_rh_bounce_gain(
    "deferred_rh_bounce_gain", "0.24", FCVAR_ARCHIVE,
    "Energy-bounded secondary-bounce gain." );

ConVar deferred_rh_receiver_offset(
    "deferred_rh_receiver_offset", "5", FCVAR_ARCHIVE,
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

ConVar deferred_rh_back_rsm_enable(
    "deferred_rh_back_rsm_enable", "1", FCVAR_ARCHIVE,
    "Renders a reverse-direction blocker RSM to capture geometry hidden from the sun-facing RSM." );

ConVar deferred_rh_visibility_inner(
    "deferred_rh_visibility_inner", "0.12", FCVAR_ARCHIVE,
    "Fully occupied radius around an RSM surface, in RH cell units." );

ConVar deferred_rh_visibility_outer(
    "deferred_rh_visibility_outer", "1.10", FCVAR_ARCHIVE,
    "Directional blocker falloff radius, in RH cell units." );

ConVar deferred_rh_visibility_strength(
    "deferred_rh_visibility_strength", "0.32", FCVAR_ARCHIVE,
    "Amplitude of the directional blocker field." );

ConVar deferred_rh_visibility_decay(
    "deferred_rh_visibility_decay", "0.35", FCVAR_ARCHIVE,
    "Distance decay used by the blocker injection pass." );

ConVar deferred_rh_geometry_strength(
    "deferred_rh_geometry_strength", "1.20", FCVAR_ARCHIVE,
    "Directional visibility strength during secondary transport." );

ConVar deferred_rh_geometry_min_transmittance(
    "deferred_rh_geometry_min_transmittance", "0.08", FCVAR_ARCHIVE,
    "Minimum secondary-bounce transmittance through blockers." );

ConVar deferred_rh_geometry_bias(
    "deferred_rh_geometry_bias", "0.05", FCVAR_ARCHIVE,
    "Visibility-field bias used to suppress receiver self-blocking." );


ConVar deferred_rh_cpu_geometry_enable(
    "deferred_rh_cpu_geometry_enable", "1", FCVAR_ARCHIVE,
    "Builds a conservative world/static-prop occupancy atlas for RH transport and shadows." );

ConVar deferred_rh_dynamic_model_blockers(
    "deferred_rh_dynamic_model_blockers", "1", FCVAR_ARCHIVE,
    "Stamps opaque client models into the RH geometry atlas each frame." );

ConVar deferred_rh_geometry_hull_scale(
    "deferred_rh_geometry_hull_scale", "0.42", FCVAR_ARCHIVE,
    "Half-extent of the conservative static-geometry cell hull, relative to RH cell size." );

ConVar deferred_rh_soft_shadow_strength(
    "deferred_rh_soft_shadow_strength", "1.05", FCVAR_ARCHIVE,
    "Strength of low-frequency RH indirect shadows." );

ConVar deferred_rh_soft_shadow_distance(
    "deferred_rh_soft_shadow_distance", "4.5", FCVAR_ARCHIVE,
    "Indirect-shadow cone length in RH cells." );

ConVar deferred_rh_soft_shadow_softness(
    "deferred_rh_soft_shadow_softness", "0.75", FCVAR_ARCHIVE,
    "Cone widening and blocker averaging for indirect penumbrae." );

ConVar deferred_rh_soft_shadow_min_visibility(
    "deferred_rh_soft_shadow_min_visibility", "0.07", FCVAR_ARCHIVE,
    "Minimum indirect visibility under RH soft shadows." );

ConVar deferred_rh_upsample_depth_scale(
    "deferred_rh_upsample_depth_scale", "0.035", FCVAR_ARCHIVE,
    "Depth rejection scale for half-resolution bilateral upsampling." );

ConVar deferred_rh_upsample_normal_power(
    "deferred_rh_upsample_normal_power", "16.0", FCVAR_ARCHIVE,
    "Normal similarity exponent for RH bilateral upsampling." );


ConVar deferred_rh_visibility_dilation(
    "deferred_rh_visibility_dilation", "0.35", FCVAR_ARCHIVE,
    "Conservative blocker expansion in RH cell units." );

ConVar deferred_rh_visibility_normal_weight(
    "deferred_rh_visibility_normal_weight", "0.55", FCVAR_ARCHIVE,
    "Influence of RSM surface orientation on blocker injection." );

ConVar deferred_rh_filter_strength(
    "deferred_rh_filter_strength", "0.70", FCVAR_ARCHIVE,
    "Visibility-aware 3D radiance reconstruction strength." );

ConVar deferred_rh_filter_fill_boost(
    "deferred_rh_filter_fill_boost", "1.75", FCVAR_ARCHIVE,
    "Neighbour contribution boost for low-confidence RH cells." );

ConVar deferred_rh_filter_energy_scale(
    "deferred_rh_filter_energy_scale", "1.20", FCVAR_ARCHIVE,
    "Rejects neighbours with strongly different radiance energy." );

ConVar deferred_rh_filter_direction_preserve(
    "deferred_rh_filter_direction_preserve", "0.65", FCVAR_ARCHIVE,
    "Anchors reliable directional SH cells during spatial reconstruction." );

ConVar deferred_rh_filter_radius(
    "deferred_rh_filter_radius", "1.0", FCVAR_ARCHIVE,
    "Spatial reconstruction radius in RH cells." );

ConVar deferred_rh_bounce_trace_width(
    "deferred_rh_bounce_trace_width", "0.35", FCVAR_ARCHIVE,
    "Cone width used while testing blockers during secondary transport." );

ConVar deferred_rh_bounce_min_confidence(
    "deferred_rh_bounce_min_confidence", "0.015", FCVAR_ARCHIVE,
    "Minimum filtered-cell confidence accepted by secondary transport." );

// Jason-style low-energy RH diffusion layered on top of the physically generated
// surface bounce. This is intentionally weak: it fills sparse low-frequency
// regions without becoming the primary transport model.
ConVar deferred_rh_diffusion_gain(
    "deferred_rh_diffusion_gain", "0.10", FCVAR_ARCHIVE,
    "Low-energy RH diffusion gain added to the physical secondary bounce." );
ConVar deferred_rh_diffusion_radius(
    "deferred_rh_diffusion_radius", "1.85", FCVAR_ARCHIVE,
    "Neighbour sampling radius in RH cells for the Jason-style diffusion layer." );
ConVar deferred_rh_diffusion_min_confidence(
    "deferred_rh_diffusion_min_confidence", "0.008", FCVAR_ARCHIVE,
    "Minimum first-bounce confidence accepted by the diffusion layer." );

ConVar deferred_rh_receiver_radius(
    "deferred_rh_receiver_radius", "0.55", FCVAR_ARCHIVE,
    "High-preset receiver reconstruction radius in RH cell units." );

ConVar deferred_rh_reconstruction_confidence_floor(
    "deferred_rh_reconstruction_confidence_floor", "0.015", FCVAR_ARCHIVE,
    "Small confidence floor preventing isolated zero-light holes." );

ConVar deferred_rh_shadow_isotropic_blend(
    "deferred_rh_shadow_isotropic_blend", "0.25", FCVAR_ARCHIVE,
    "Adds local isotropic blocking when radiance direction is ambiguous." );


// Hemisphere skylight projected into the RH volume. The colours come from the
// global light's ambient-high/ambient-low values.
ConVar deferred_rh_sky_enable(
    "deferred_rh_sky_enable", "1", FCVAR_ARCHIVE,
    "Injects geometry-shadowed upper/lower hemisphere skylight into RH." );
ConVar deferred_rh_sky_intensity(
    "deferred_rh_sky_intensity", "0.55", FCVAR_ARCHIVE,
    "Overall hemisphere skylight intensity inside RH." );
ConVar deferred_rh_sky_upper_scale(
    "deferred_rh_sky_upper_scale", "1.0", FCVAR_ARCHIVE,
    "Multiplier for global ambient-high hemisphere colour." );
ConVar deferred_rh_sky_lower_scale(
    "deferred_rh_sky_lower_scale", "0.30", FCVAR_ARCHIVE,
    "Multiplier for global ambient-low hemisphere colour; true lower light is also produced by surface bounce." );
ConVar deferred_rh_sky_occlusion(
    "deferred_rh_sky_occlusion", "1.25", FCVAR_ARCHIVE,
    "Geometry extinction applied to RH hemisphere skylight." );
ConVar deferred_rh_sky_trace_distance(
    "deferred_rh_sky_trace_distance", "8.0", FCVAR_ARCHIVE,
    "Maximum local hemisphere visibility distance in RH cells." );
ConVar deferred_rh_sky_replace_global_ambient(
    "deferred_rh_sky_replace_global_ambient", "1", FCVAR_ARCHIVE,
    "Uses ambient-high/ambient-low as shadowed RH sky only and suppresses the duplicate unshadowed global ambient pass." );

// Surface-aware secondary bounce.
ConVar deferred_rh_surface_bounce_gain(
    "deferred_rh_surface_bounce_gain", "0.85", FCVAR_ARCHIVE,
    "Lambertian surface re-emission gain before secondary transport." );
ConVar deferred_rh_surface_radius(
    "deferred_rh_surface_radius", "1.35", FCVAR_ARCHIVE,
    "RSM surface-material gather radius in RH cell units." );
ConVar deferred_rh_surface_min_coverage(
    "deferred_rh_surface_min_coverage", "0.04", FCVAR_ARCHIVE,
    "Minimum RSM surface coverage required to generate a secondary bounce." );

// Receiver/cone anti-self-intersection and independent shadow extinction.
ConVar deferred_rh_shadow_extinction(
    "deferred_rh_shadow_extinction", "1.30", FCVAR_ARCHIVE,
    "Optical extinction for low-frequency indirect shadows." );
ConVar deferred_rh_shadow_start_cells(
    "deferred_rh_shadow_start_cells", "0.20", FCVAR_ARCHIVE,
    "Distance from the cleared receiver at which the indirect-shadow cone starts." );
ConVar deferred_rh_shadow_receiver_clearance(
    "deferred_rh_shadow_receiver_clearance", "0.30", FCVAR_ARCHIVE,
    "Normal-space receiver clearance in RH cells used to avoid floor/wall self-shadowing." );
ConVar deferred_rh_shadow_contact_strength(
    "deferred_rh_shadow_contact_strength", "1.0", FCVAR_ARCHIVE,
    "Near-field blocker weight in RH indirect shadows." );
ConVar deferred_rh_shadow_far_strength(
    "deferred_rh_shadow_far_strength", "0.65", FCVAR_ARCHIVE,
    "Far-field blocker weight in RH indirect shadows." );

// RH8: decoupled 64^3 SDF shadow field.
ConVar deferred_rh_shadow_sdf_enable(
    "deferred_rh_shadow_sdf_enable", "1", FCVAR_ARCHIVE,
    "Uses the dedicated 64^3 Euclidean-distance visibility field for RH shadows." );
ConVar deferred_rh_shadow_sdf_steps(
    "deferred_rh_shadow_sdf_steps", "6", FCVAR_ARCHIVE,
    "First-bounce explicit SDF cone steps. Legacy-FXC-safe range is 3..6." );
ConVar deferred_rh_shadow_sdf_second_steps(
    "deferred_rh_shadow_sdf_second_steps", "3", FCVAR_ARCHIVE,
    "Second-bounce explicit SDF cone steps. Legacy-FXC-safe range is 1..3." );
ConVar deferred_rh_shadow_sdf_step_scale(
    "deferred_rh_shadow_sdf_step_scale", "0.82", FCVAR_ARCHIVE,
    "Fraction of sampled SDF distance used as the next shadow step." );
ConVar deferred_rh_shadow_sdf_min_step(
    "deferred_rh_shadow_sdf_min_step", "0.35", FCVAR_ARCHIVE,
    "Minimum SDF step in 64^3 shadow cells." );
ConVar deferred_rh_shadow_sdf_max_step(
    "deferred_rh_shadow_sdf_max_step", "2.0", FCVAR_ARCHIVE,
    "Maximum SDF step in 64^3 shadow cells; limits skipping over dynamic OBB blockers." );
ConVar deferred_rh_shadow_sdf_softness(
    "deferred_rh_shadow_sdf_softness", "1.15", FCVAR_ARCHIVE,
    "SDF penumbra scale. RH anisotropy automatically narrows directional shadows." );
ConVar deferred_rh_shadow_second_strength(
    "deferred_rh_shadow_second_strength", "0.72", FCVAR_ARCHIVE,
    "Receiver shadow strength applied independently to the transported second-bounce field." );

// RH9: independent geometry surface guide and adaptive transport.
ConVar deferred_rh_surface_cache_enable(
    "deferred_rh_surface_cache_enable", "1", FCVAR_ARCHIVE,
    "Enables the CPU/SDF-derived surface normal/coverage guide independent of RSM visibility." );
ConVar deferred_rh_surface_cache_blend(
    "deferred_rh_surface_cache_blend", "0.70", FCVAR_ARCHIVE,
    "How strongly the independent surface guide repairs missing/incoherent RSM surface normals." );
ConVar deferred_rh_surface_fallback_albedo(
    "deferred_rh_surface_fallback_albedo", "0.52", FCVAR_ARCHIVE,
    "Neutral diffuse albedo used only for near-surface cells that have geometry coverage but no RSM albedo." );
ConVar deferred_rh_cell_relocation(
    "deferred_rh_cell_relocation", "0.32", FCVAR_ARCHIVE,
    "Maximum stable RH sample relocation along the SDF surface normal, in radiance-cell units." );
ConVar deferred_rh_adaptive_gather(
    "deferred_rh_adaptive_gather", "1", FCVAR_ARCHIVE,
    "Adapts first-bounce RSM gather radius from distance to geometry." );
ConVar deferred_rh_adaptive_gather_near(
    "deferred_rh_adaptive_gather_near", "3.5", FCVAR_ARCHIVE,
    "RSM gather radius in radiance cells near geometry." );
ConVar deferred_rh_adaptive_gather_far(
    "deferred_rh_adaptive_gather_far", "5.5", FCVAR_ARCHIVE,
    "RSM gather radius in radiance cells in open space." );
ConVar deferred_rh_adaptive_diffusion(
    "deferred_rh_adaptive_diffusion", "1", FCVAR_ARCHIVE,
    "Adapts Jason-style secondary diffusion radius from geometry clearance." );
ConVar deferred_rh_adaptive_diffusion_near(
    "deferred_rh_adaptive_diffusion_near", "0.90", FCVAR_ARCHIVE,
    "Diffusion radius in radiance cells near geometry." );
ConVar deferred_rh_adaptive_diffusion_far(
    "deferred_rh_adaptive_diffusion_far", "2.50", FCVAR_ARCHIVE,
    "Diffusion radius in radiance cells in open space." );

// RH10 material-aware surface cache, classification and relocation.
ConVar deferred_rh_surface_material_cache(
    "deferred_rh_surface_material_cache", "1", FCVAR_ARCHIVE,
    "Use material-average static surface albedo captured from Source materials." );
ConVar deferred_rh_surface_material_scale(
    "deferred_rh_surface_material_scale", "1.0", FCVAR_ARCHIVE,
    "Scale material-average albedo used by the static RH surface cache." );
ConVar deferred_rh_cell_classification(
    "deferred_rh_cell_classification", "1", FCVAR_ARCHIVE,
    "Classify solid/surface/empty RH cells and reject invalid transport sources." );
ConVar deferred_rh_relocation_clearance(
    "deferred_rh_relocation_clearance", "0.55", FCVAR_ARCHIVE,
    "Desired free-space clearance, in RH cells, for SDF-guided cell relocation." );
ConVar deferred_rh_receiver_visibility_weight(
    "deferred_rh_receiver_visibility_weight", "0.85", FCVAR_ARCHIVE,
    "Reject receiver reconstruction samples separated by nearby blocker geometry." );
ConVar deferred_rh_rsm_kernel_energy(
    "deferred_rh_rsm_kernel_energy", "1.0", FCVAR_ARCHIVE,
    "Energy calibration for the normalized fixed 16-sample RSM disk kernel." );

// Source-aware coarse open-sky cache. SURF_SKY/no-hit rays are considered open.
ConVar deferred_rh_sky_cache_enable(
    "deferred_rh_sky_cache_enable", "1", FCVAR_ARCHIVE,
    "Use a coarse world-space Source sky-visibility cache for RH hemisphere input." );
ConVar deferred_rh_sky_cache_blend(
    "deferred_rh_sky_cache_blend", "0.85", FCVAR_ARCHIVE,
    "Blend cached Source sky visibility over local RH geometry visibility." );
ConVar deferred_rh_sky_cache_trace_distance(
    "deferred_rh_sky_cache_trace_distance", "4096", FCVAR_ARCHIVE,
    "World-space trace distance for the coarse Source sky visibility cache." );

// RH11 radiance and blocker hierarchies.
ConVar deferred_rh_hierarchy_enable(
    "deferred_rh_hierarchy_enable", "1", FCVAR_ARCHIVE,
    "Enable 20^3/10^3/5^3 radiance hierarchy and broad-scale reconstruction." );
ConVar deferred_rh_hierarchy_diffusion_blend(
    "deferred_rh_hierarchy_diffusion_blend", "0.55", FCVAR_ARCHIVE,
    "Blend coarse 20^3 radiance into wide diffusion samples in open space." );
ConVar deferred_rh_hierarchy_receiver_fill(
    "deferred_rh_hierarchy_receiver_fill", "0.22", FCVAR_ARCHIVE,
    "Maximum broad radiance hierarchy fill for low-confidence receiver cells." );
ConVar deferred_rh_shadow_hierarchy_enable(
    "deferred_rh_shadow_hierarchy_enable", "1", FCVAR_ARCHIVE,
    "Use conservative 32^3/16^3 blocker mips for wide/far SDF shadow steps." );

ConVar deferred_rh_debug_mode(
    "deferred_rh_debug_mode", "0", FCVAR_NONE,
    "RH debug mode. 0=off, 1=volume bounds/RSM status." );

// Legacy compatibility controls.
ConVar deferred_rh_min_visibility(
    "deferred_rh_min_visibility", "0.02", FCVAR_ARCHIVE,
    "Legacy compatibility setting; retained for existing configs." );
ConVar deferred_rh_validity_boost(
    "deferred_rh_validity_boost", "1.0", FCVAR_ARCHIVE,
    "Legacy compatibility setting; RH7 derives confidence from SH energy." );
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
