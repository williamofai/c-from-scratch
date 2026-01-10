# c-from-scratch

**Learn to build safety-critical systems in C.**

Not "Hello World". Real kernels. Mathematical rigour. Zero dependencies.

> *"Math → Structs → Code"*

## Philosophy

Most tutorials teach you to write code that *seems* to work.  
This course teaches you to write code that *provably* works.

**The method:**
1. Define the problem mathematically
2. Prove correctness formally
3. Design structs that embody the proof
4. Transcribe the math into C
5. Test against the contracts

```
┌─────────────────────────────────────────────────────────────┐
│                    THE APPROACH                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   Problem  ──►  Math Model  ──►  Proof  ──►  Structs        │
│                                                │            │
│                                                ▼            │
│                           Verification  ◄──  Code           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## The Seven Foundation Modules

| Module | Question | Role | Tests |
|--------|----------|------|-------|
| [Pulse](./projects/pulse/) | Does it exist? | Sensor | ✓ |
| [Baseline](./projects/baseline/) | Is it normal? | Sensor | 18/18 |
| [Timing](./projects/timing/) | Is it regular? | Sensor | ✓ |
| [Drift](./projects/drift/) | Is it trending toward failure? | Sensor | 15/15 |
| [Consensus](./projects/consensus/) | Which sensor to trust? | Judge | 17/17 |
| [Pressure](./projects/pressure/) | How to handle overflow? | Buffer | 16/16 |
| [Mode](./projects/mode/) | What do we do about it? | Captain | 17/17 |

Plus: [Integration Example](./projects/integration/) — All modules working together.

---

## The Safety Stack

```
┌─────────────────────────────────────────────────────────────────────┐
│                    MODULE 7: MODE MANAGER                           │
│                       "The Captain"                                 │
│   Decides: What mode? What actions allowed?                         │
└─────────────────────────────────────────────────────────────────────┘
                              ↑
┌─────────────────────────────────────────────────────────────────────┐
│                    MODULE 6: PRESSURE                               │
│                       "The Buffer"                                  │
└─────────────────────────────────────────────────────────────────────┘
                              ↑
┌─────────────────────────────────────────────────────────────────────┐
│                    MODULE 5: CONSENSUS                              │
│                       "The Judge"                                   │
└─────────────────────────────────────────────────────────────────────┘
                              ↑
          ┌───────────────────┼───────────────────┐
          ▼                   ▼                   ▼
┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
│   CHANNEL 0     │ │   CHANNEL 1     │ │   CHANNEL 2     │
│  Pulse → Base   │ │  Pulse → Base   │ │  Pulse → Base   │
│  → Timing       │ │  → Timing       │ │  → Timing       │
│  → Drift        │ │  → Drift        │ │  → Drift        │
│   "Sensors"     │ │   "Sensors"     │ │   "Sensors"     │
└─────────────────┘ └─────────────────┘ └─────────────────┘
```

---

## Module Overview

### Modules 1-4: The Sensors

| Module | Question | Key Insight |
|--------|----------|-------------|
| **Pulse** | Is it alive? | Existence must be proven continuously |
| **Baseline** | Is it normal? | "Normal" must be learned, not assumed |
| **Timing** | Is it regular? | Jitter indicates system stress |
| **Drift** | Is it trending? | "Temperature is normal but rising too fast" |

### Module 5: The Judge

**Consensus** — Triple Modular Redundancy voting.

> *"With THREE clocks, we can outvote the liar."*

### Module 6: The Buffer

**Pressure** — Bounded queue with backpressure.

> *"Only bounded queues let you choose deliberately."*

### Module 7: The Captain

**Mode** — System orchestrator with permissions matrix.

> *"Sensors report. The Captain decides."*

---

## Core Properties

Every module is:

- **Closed** — No external dependencies at runtime
- **Total** — Handles all possible inputs
- **Deterministic** — Same inputs → Same outputs
- **O(1)** — Constant time, constant space
- **Contract-defined** — Behaviour is specified, not implied

---

## Getting Started

```bash
# Clone
git clone https://github.com/williamofai/c-from-scratch.git
cd c-from-scratch

# Try any module
cd projects/pulse
make && make test && make demo

# Or run the full integration
cd projects/integration
make run
```

Start with [Pulse Lesson 1: The Problem](./projects/pulse/lessons/01-the-problem/LESSON.md).

---

## Specification

See [SPEC.md](./SPEC.md) for the complete framework specification:
- Core principles
- Module structure
- Contract definitions
- Composition rules
- Certification alignment (DO-178C, IEC 62304, ISO 26262)

---

## Who This Is For

- Developers building safety-critical software
- Systems programmers who want provable correctness
- Students learning C the rigorous way
- Anyone tired of "it works on my machine"

## Prerequisites

- Basic C syntax (variables, functions, structs)
- Comfort with command line
- Willingness to think before coding

---

## Author

**William Murray** — 30 years UNIX systems engineering

- GitHub: [@williamofai](https://github.com/williamofai)
- LinkedIn: [William Murray](https://www.linkedin.com/in/williammurray1967/)
- Website: [speytech.com](https://speytech.com)

## License

MIT — See [LICENSE](./LICENSE)

---

> *"Sensors report. The Captain decides."*
