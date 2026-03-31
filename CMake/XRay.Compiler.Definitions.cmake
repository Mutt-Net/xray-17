include_guard()

# Configure compile definitions
add_compile_definitions(
  # Suppress deprecation errors
  _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
)

# Activate interprocedural optimization for release builds.
# RelWithDebInfo is excluded: IPO injects /GL+/LTCG per-target and the LTCG
# pass-2 backend exhausts available RAM on this codebase. This is a dev-
# iteration configuration; whole-program optimisation is not required.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_PROFILED On)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE On)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO Off)
