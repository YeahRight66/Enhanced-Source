// Included exactly once by deferred_client_common.cpp.
//
// Production daylight GI intentionally has a small, bounded control surface.
// Algorithmic/SDF constants live in code so old RH configs cannot create
// non-physical energy, light leaks, or unsupported shader permutations.

ConVar deferred_gi_enable(
    "deferred_gi_enable", "1", FCVAR_ARCHIVE,
    "Enable two-level daylight global illumination.", true, 0.0f, true, 1.0f );
ConVar deferred_gi_quality(
    "deferred_gi_quality", "2", FCVAR_ARCHIVE,
    "Daylight GI quality: 0=low, 1=balanced, 2=high.", true, 0.0f, true, 2.0f );
ConVar deferred_gi_intensity(
    "deferred_gi_intensity", "1.0", FCVAR_ARCHIVE,
    "Physically bounded final daylight-GI intensity.", true, 0.0f, true, 2.0f );
ConVar deferred_gi_bounce_intensity(
    "deferred_gi_bounce_intensity", "1.0", FCVAR_ARCHIVE,
    "Secondary diffuse-bounce intensity.", true, 0.0f, true, 1.0f );
ConVar deferred_gi_dynamic_blockers(
    "deferred_gi_dynamic_blockers", "1", FCVAR_ARCHIVE,
    "Stamp moving studio and brush bounds into the GI blocker overlay.", true, 0.0f, true, 1.0f );
ConVar deferred_gi_debug(
    "deferred_gi_debug", "0", FCVAR_NONE,
    "GI debug: 0=off, 1=first, 2=second, 3=clips, 4=SH, 5=confidence, 6=occupancy, 7=SDF, 8=surface, 9=sun injection visibility, 10=receiver rejection.",
    true, 0.0f, true, 10.0f );
ConVar deferred_gi_freeze(
    "deferred_gi_freeze", "0", FCVAR_CHEAT,
    "Freeze GI clip origins and static caches for inspection.", true, 0.0f, true, 1.0f );
ConVar deferred_gi_stats(
    "deferred_gi_stats", "0", FCVAR_NONE,
    "Display daylight-GI cache timing and rebuild counters.", true, 0.0f, true, 1.0f );
