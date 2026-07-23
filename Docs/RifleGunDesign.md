# RifleGun prototype

## Gameplay

- Hold left mouse button for continuous fire.
- Two scene-component muzzles fire alternately.
- Infinite ammunition, no reload, heat or spin-up.
- Default stats: 50 damage, 20 shots/s, 2000 m range, 0.25 degree spread.
- Default tracer: 2000 m/s visual speed, 5 m length and 5 cm width.
- The existing Pawn capsule is the temporary hit target. Dedicated weapon hitboxes are deferred.

## Networking

- The owning client immediately creates a predicted cosmetic shot.
- The server independently performs the authoritative Visibility line trace and applies damage.
- An unreliable multicast sends the authoritative visual path to other clients.
- Tracer actors are local-only, have no collision and are never replicated.
- Server rewind is intentionally deferred.

## Visual setup

Create `BP_RifleTracer` as a child of `ARifleTracerVisual`, then assign a cube mesh and an
Unlit/Emissive material to its inherited `TracerMesh`. The mesh must point along local X.
Assign the Blueprint to `RifleGun -> Tracer Visual Class` in `BP_PlayerPlane`.

Optional Niagara slots on `RifleGun`:

- `Muzzle Flash Effect`
- `Impact Effect`

The tracer stops at the trace result and never controls collision or damage.
