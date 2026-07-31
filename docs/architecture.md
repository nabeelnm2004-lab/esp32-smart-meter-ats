# Architecture Rules

## Folder Structure

src/
├── core/
├── hardware/
├── network/
├── scheduler/
├── storage/
├── ui/
└── utilities/

## Rules

- Every module must have one responsibility.
- Each module should contain one `.cpp` and one `.h`.
- Do not mix unrelated functionality.
- Keep business logic separate from hardware logic.
- Keep UI separate from backend logic.
- Network code must never directly control hardware.
- Scheduler communicates through interfaces, not direct dependencies.
- Prefer composition over inheritance.
- Extend existing modules instead of creating duplicates.
- Do not reorganize existing folders without permission.