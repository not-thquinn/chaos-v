# Chaos V

An event-driven desktop explorer for a two-segment, falling-ball chaotic system.

## What it does

- Uses MPFR floating-point arithmetic for physics. Fractal workers dispatch
  common precision ranges to fixed, stack-backed 128-, 257-, and 513-bit
  engines; uncommon higher precisions retain the variable backend.
- Computes only discrete events: ball/ball impacts, ball/segment impacts,
  despawns, and scheduled spawns.
- Treats a finite segment as two offset-line faces plus two endpoint circles.
  Endpoint-circle collision times are found by high-precision real-root
  isolation of the resulting quartic, rather than unstable Ferrari radicals.
- Renders the angle parameter plane in CPU worker threads, progressively by
  tiles, and exports the current image as PNG.
- Optionally shades each period by a causal stability certificate derived from
  essential collisions and relevant cross-boundary near misses.
- Lets a click in the parameter plane run and animate the corresponding
  simulation.
- Shift-clicks rendered pixels at their exact source coordinates and can copy
  or restore the complete selected simulation as JSON.
- Persists completed fractal layers between sessions, grouped by the physical
  parameters that determine the underlying simulation.
- Produces numbered parameter sweeps, animated fractal zooms, and perfectly
  looping simulation-frame sequences. Long bulk renders can be paused and
  resumed across application restarts.

Period detection follows the discrete ball-ball collision graph. It expands
the smallest prefix of ball IDs until no collision component crosses the
prefix boundary; when every ball in that closed prefix has despawned, its
length is the period. For periodic travelling interaction chains that never
close, a targeted suffix detector compares repeated relative-partner patterns
and compact exit fingerprints while allowing an initial startup transient.

## Build

Install development packages for Qt 6 Widgets, Boost headers,
GMP, MPFR, CMake, and a C++20 compiler. On a package-manager based system:

```text
cmake -S . -B build
cmake --build build --config Release
```

The executable is named `chaos_v` (or `chaos_v.exe` on Windows).

GMP/MPFR are required by default so a production fractal cannot silently use
lower precision. Set `-DCHAOSV_REQUIRE_MPFR=OFF` explicitly to build the
`long double` fallback for development.

## Notes

The UI's precision value is specified in binary bits. The variable MPFR path
adds eight binary guard bits and converts to the smallest Boost decimal
precision that satisfies that working precision. Interactive previews can run
while the dedicated fractal-rendering pool is active.
