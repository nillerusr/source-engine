# OpenCode Loop Prompt

You are working in a repo whose goal is to build a Rust Source-compatible runtime alongside or from this Source Engine codebase.

Project goal:
Create a Rust engine/runtime that can load Source Engine content well enough to run maps with compatible BSP geometry, VPK-mounted assets, VMT/VTF materials, simple game entities, a Source-like console/CVar system, and a menu/VGUI-like UI.

You must work incrementally. Do not attempt broad rewrites. Do not invent large systems unless the current task requires them.

## State Files

Use these files as persistent memory between runs:

- `PROJECT_STATE.md`: current status, architecture decisions, known limitations.
- `ROADMAP.md`: ordered milestones and task backlog.
- `COMPATIBILITY.md`: supported Source formats/entities/features.
- `TASKS.md`: active, queued, blocked, and completed tasks.
- `DEVLOG.md`: append-only progress log.
- `DECISIONS.md`: append-only architectural decisions.
- `TESTING.md`: how to run tests and what currently passes/fails.

## Before Doing Any Work

1. Read all state files if they exist.
2. Inspect the repo structure.
3. Identify the next smallest unblocked task from `TASKS.md` or `ROADMAP.md`.
4. If no state files exist, create them with an initial plan.

## Work Loop

1. Pick exactly one small task.
2. State the selected task in `DEVLOG.md`.
3. Implement only that task.
4. Add or update tests/fixtures where practical.
5. Run formatting and relevant tests.
6. Update `PROJECT_STATE.md`, `TASKS.md`, `COMPATIBILITY.md`, and `TESTING.md`.
7. Append a short `DEVLOG.md` entry with:
   - task completed
   - files changed
   - commands run
   - tests passing/failing
   - next recommended task
8. Stop.

## Engineering Rules

- Prefer small, testable crates/modules.
- Prefer parsing/dump tools before runtime features.
- Use Rust 2021 or newer.
- Avoid `unsafe` unless absolutely necessary and documented in `DECISIONS.md`.
- Do not fake compatibility. If something is placeholder, mark it clearly in `COMPATIBILITY.md`.
- Do not implement unrelated features.
- Do not delete user work.
- Do not make sweeping formatting changes outside files touched for the task.
- Treat the original Source Engine code and tools as reference material, not as something to port wholesale in one pass.

## Suggested Architecture

- `crates/source_math`
- `crates/source_bsp`
- `crates/source_vpk`
- `crates/source_vmt`
- `crates/source_vtf`
- `crates/source_entity`
- `crates/source_render`
- `crates/source_vgui`
- `crates/source_console`
- `crates/source_app`

## Preferred Libraries

- `glam` for math
- `wgpu` for rendering
- `winit` for window/input
- `egui` for early menu/debug UI
- `serde` for structured dumps/tests
- `clap` for CLI tools
- `tracing` for logging
- `insta` or JSON fixtures for snapshot-style parser tests

## Initial Milestone Order

1. Workspace/crate layout.
2. BSP header and lump directory parser.
3. BSP entity lump parser.
4. BSP planes, vertices, edges, surfedges, faces.
5. Basic map dump CLI.
6. VPK filesystem mounting.
7. VMT KeyValues parser.
8. VTF loading or placeholder texture path.
9. Render BSP world geometry.
10. Free camera.
11. `info_player_start` and `worldspawn` support.
12. Console and CVar registry.
13. Basic Source-like menu.
14. Player controller with BSP collision.
15. `trigger_once` / `trigger_multiple`.
16. Entity I/O outputs.
17. `func_button` / `func_door`.
18. HUD basics.

## Task Size Rule

A task should usually be completable in under 1-2 hours and should leave the repo in a runnable or testable state.

If uncertain:

- Prefer adding a dump/inspection tool.
- Prefer parser tests over runtime guesses.
- Prefer documenting a limitation over pretending it works.

## Starter `TASKS.md`

If `TASKS.md` does not exist, create this:

```md
# Tasks

## Active
- [ ] Create Rust workspace layout for Source-compatible runtime.

## Queue
- [ ] Add `source_math` crate with `Vec3`, `QAngle`, planes, bounds helpers using `glam`.
- [ ] Add `source_bsp` crate and parse BSP header.
- [ ] Parse BSP lump directory.
- [ ] Add `bsp_dump` CLI that prints header/lump metadata as JSON.
- [ ] Parse BSP entity lump as KeyValues-style entities.
- [ ] Add entity dump command.
- [ ] Parse planes lump.
- [ ] Parse vertices lump.
- [ ] Parse edges and surfedges.
- [ ] Parse faces.
- [ ] Resolve texinfo and texdata.
- [ ] Add placeholder renderer with `wgpu`.
- [ ] Render untextured BSP world geometry.
- [ ] Add VMT parser.
- [ ] Add VTF texture path/loading placeholder.
- [ ] Render textured BSP faces.
- [ ] Add free camera.
- [ ] Add console and CVar registry.
- [ ] Add Source-like main menu.
- [ ] Implement `worldspawn`.
- [ ] Implement `info_player_start`.
- [ ] Implement basic player movement.
- [ ] Implement BSP collision traces.
- [ ] Implement `trigger_once`.
- [ ] Implement Source-style entity output dispatch.

## Blocked

## Done
```

## Starter `PROJECT_STATE.md`

If `PROJECT_STATE.md` does not exist, create this:

```md
# Project State

## Goal
Build a Rust Source-compatible runtime capable of loading and running Source maps with compatible assets, basic entities, menu UI, console/CVars, and game-layer behavior.

## Current Focus
Early engine foundation: workspace, parsers, dump tools, tests.

## Architecture
The project is split into small crates for formats, rendering, entity/game logic, UI, and application runtime.

## Compatibility Target
Initial target: Source SDK 2013 Singleplayer-style BSP maps and assets.

## Non-Goals For Now
- Full Source multiplayer networking
- Full NPC AI
- Full Source shader parity
- Save/load
- Demo playback
- Complete VGUI port
- Direct C++ game DLL compatibility

## Current Limitations
Everything is initially incomplete until implemented and tested.
```

## Agent Instruction

Now begin the loop.
