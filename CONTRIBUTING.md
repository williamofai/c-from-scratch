# Contributing to c-from-scratch

Thank you for your interest in contributing!

## Philosophy

This is an educational repository for safety-critical C programming. Contributions should:

1. **Maintain rigour** — No hand-waving. Proofs matter.
2. **Preserve clarity** — Beginner-friendly explanations
3. **Follow the method** — Math → Structs → Code

## Current Status

### Foundation Modules (Complete)

| Module | Question | Tests | Status |
|--------|----------|-------|--------|
| Pulse | Does it exist? | ✓ | Complete |
| Baseline | Is it normal? | 18/18 | Complete |
| Timing | Is it regular? | ✓ | Complete |
| Drift | Is it trending? | 15/15 | Complete |
| Consensus | Which to trust? | 17/17 | Complete |
| Pressure | Handle overflow? | 16/16 | Complete |

### Also Available

- **Integration Example** — All 6 modules working together
- **SPEC.md** — Framework specification

## What We Need

### High Priority
- Typo fixes and clarifications
- Additional test cases for edge conditions
- Platform-specific build instructions (Windows, macOS)
- Translations (if you're fluent)

### Medium Priority
- Improved diagrams and visualisations
- Additional exercises per module
- Real-world application examples
- Performance benchmarks

### Discussions Welcome
- Alternative mathematical approaches
- Additional failure mode analysis
- Real-world war stories
- Ideas for Module 7+ (State Machine? Scheduler?)

## How to Contribute

### Simple Fixes
1. Fork the repository
2. Make your change
3. Submit a pull request with clear description

### Larger Changes
1. Open an issue first to discuss
2. Wait for feedback before investing time
3. Follow the existing style and structure

### New Modules

New modules must follow the established pattern:

```
projects/module-name/
├── include/
│   └── module.h          # API + contracts in comments
├── src/
│   ├── module.c          # Implementation
│   └── main.c            # Demo program
├── tests/
│   └── test_module.c     # Contract + invariant tests
├── lessons/
│   ├── 01-the-problem/
│   │   └── LESSON.md     # Why this matters
│   ├── 02-mathematical-model/
│   │   └── LESSON.md     # Formal specification
│   ├── 03-structs/
│   │   └── LESSON.md     # Data encoding
│   ├── 04-code/
│   │   └── LESSON.md     # Implementation walkthrough
│   ├── 05-testing/
│   │   └── LESSON.md     # Proof harness
│   └── 06-composition/
│       └── LESSON.md     # Integration with other modules
├── Makefile
├── README.md
└── .gitignore
```

See [SPEC.md](./SPEC.md) for complete requirements.

## Style Guidelines

### C Code

```makefile
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c11 -O2
```

- C11 standard
- `snake_case` for functions and variables
- `UPPER_CASE` for constants and enums
- Comments explain *why*, not *what*
- Every function must be **total** (defined for all inputs)

### Naming Conventions

Follow the module pattern:
```c
module_init()      // Initialise
module_update()    // Step the FSM
module_state()     // Query current state (inline)
module_reset()     // Reset to initial
```

### Forbidden Constructs

| Construct | Reason |
|-----------|--------|
| `malloc` / `free` | Non-deterministic timing |
| Recursion | Unbounded stack |
| Global variables | Hidden state |
| `goto` | Unstructured control flow |

### Struct Design

- Every field must trace to a mathematical requirement
- Configuration is immutable after init
- Faults are sticky until explicit reset
- Derived values belong in result structs, not state

### Markdown

- One sentence per line (easier diffs)
- Use ATX headers (`#`, `##`, etc.)
- Code blocks with language specifier

### Commit Messages

Present tense, first line under 50 characters:

```
Add Module 7: Scheduler - Deterministic Task Dispatch

- Closed, total, deterministic FSM
- Priority-based with deadline awareness
- N/N contract tests passing
- Six lessons from problem to composition
```

## Testing Standards

Every module needs:

1. **Contract tests** — One test per proven contract
2. **Invariant tests** — Verify structural guarantees
3. **Edge case tests** — Zero, overflow, NaN, reset
4. **Fuzz tests** — Random input, 100,000+ iterations

Test output format:
```
╔════════════════════════════════════════════════════════════════╗
║           MODULE Contract Test Suite                           ║
╚════════════════════════════════════════════════════════════════╝

Contract Tests:
  [PASS] CONTRACT-1: Description
  [PASS] CONTRACT-2: Description

Fuzz Tests:
  [PASS] Fuzz: 100000 random inputs, invariants held

══════════════════════════════════════════════════════════════════
  Results: N/N tests passed
══════════════════════════════════════════════════════════════════
```

## Code of Conduct

Be kind. Be helpful. Assume good intent.

This is a learning environment. Questions are welcome, no matter how basic.

## Questions?

Open an issue with the "question" label.

---

> *"Math → Structs → Code"*
