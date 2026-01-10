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

## The Six Foundation Modules

| Module | Question | Tests | Status |
|--------|----------|-------|--------|
| [Pulse](./projects/pulse/) | Does it exist? | ✓ | Complete |
| [Baseline](./projects/baseline/) | Is it normal? | 18/18 | Complete |
| [Timing](./projects/timing/) | Is it regular? | ✓ | Complete |
| [Drift](./projects/drift/) | Is it trending toward failure? | 15/15 | Complete |
| [Consensus](./projects/consensus/) | Which sensor to trust? | 17/17 | Complete |
| [Pressure](./projects/pressure/) | How to handle overflow? | 16/16 | Complete |

Plus: [Integration Example](./projects/integration/) — All 6 modules working together.

---

## Module Overview

### Module 1: Pulse — Heartbeat Liveness Monitor

*"Does something exist in time?"*

A tiny, provably-correct state machine that answers: *"Is this process alive?"*

**Contracts:** Soundness, Liveness, Stability

---

### Module 2: Baseline — Statistical Normality Monitor

*"Is what's happening normal?"*

EMA-based anomaly detection with O(1) memory.

**Contracts:** Convergence, Sensitivity, Stability, Spike Resistance

---

### Module 3: Timing — Regularity Monitor

*"Are events arriving on schedule?"*

Detects timing jitter and missed deadlines.

**Contracts:** Jitter detection within tolerance

---

### Module 4: Drift — Rate & Trend Detection

*"Is the value moving toward failure?"*

Damped derivative via EMA of slope. Catches "temperature is normal but rising too fast."

**Contracts:** Bounded slope, Noise immunity, TTF accuracy, Spike resistance

---

### Module 5: Consensus — Triple Modular Redundancy

*"Which of three sensors should we trust?"*

Mid-value selection (median) for Byzantine fault tolerance.

**Contracts:** Single-fault tolerance, Bounded output, Deterministic, Degradation awareness

> *"With THREE clocks, we can outvote the liar."*

---

### Module 6: Pressure — Bounded Queue with Backpressure

*"What do we do when messages arrive faster than we can process?"*

Ring buffer with three overflow policies: REJECT, DROP_OLDEST, DROP_NEWEST.

**Contracts:** Bounded memory, No data loss, FIFO ordering, Pressure signal accuracy

> *"Only bounded queues let you choose deliberately."*

---

## Integration Example

All six modules composing into a complete safety monitoring system:

```
┌─────────────────────────────────────────────────────────────────────┐
│                        SAFETY MONITOR                               │
│                                                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐               │
│  │  SENSOR 0    │  │  SENSOR 1    │  │  SENSOR 2    │               │
│  │ Pulse→Base   │  │ Pulse→Base   │  │ Pulse→Base   │               │
│  │ →Timing      │  │ →Timing      │  │ →Timing      │               │
│  │ →Drift       │  │ →Drift       │  │ →Drift       │               │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘               │
│         └─────────────────┼─────────────────┘                       │
│                           ▼                                         │
│                    ┌──────────────┐                                 │
│                    │  CONSENSUS   │  (TMR voting)                   │
│                    └──────┬───────┘                                 │
│                           ▼                                         │
│                    ┌──────────────┐                                 │
│                    │   PRESSURE   │  (bounded queue)                │
│                    └──────┬───────┘                                 │
│                           ▼                                         │
│                        OUTPUT                                       │
└─────────────────────────────────────────────────────────────────────┘
```

**Result:** 0.2% error despite Byzantine sensor fault and complete sensor failure.

```bash
cd projects/integration
make run
```

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

> *"Good systems don't trust. They verify. Better systems don't verify once. They vote."*
