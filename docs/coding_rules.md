# General Coding Rules

## Code Quality

- Follow SOLID principles.
- Follow DRY (Don't Repeat Yourself).
- Follow KISS (Keep It Simple).
- Prefer readability over clever code.
- Keep functions small.
- Keep files reasonably small.
- Remove duplicate logic.
- Avoid unnecessary abstraction.

## Naming

- Use descriptive names.
- Never use meaningless variable names.
- Avoid abbreviations unless standard.

## Functions

- One responsibility per function.
- Keep nesting shallow.
- Prefer early returns.
- Avoid long parameter lists.

## Constants

- Never hardcode values.
- Store configurable values in config.h.

## Error Handling

- Validate all input.
- Check return values.
- Fail safely.
- Never ignore errors.

## Memory

- Avoid unnecessary heap allocation.
- Avoid memory leaks.
- Minimize RAM usage.
- Reuse buffers whenever possible.

## Performance

- Avoid unnecessary loops.
- Cache repeated calculations.
- Prefer efficient algorithms.

## Comments

- Explain WHY, not WHAT.
- Remove outdated comments.
- Keep comments synchronized with code.

## Security

- Validate all external input.
- Never expose secrets.
- Never hardcode passwords.

## Modification Rules

- Preserve existing formatting.
- Do not rewrite unrelated code.
- Modify only affected files.
- Preserve backward compatibility.