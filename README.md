# C-From-Scratch

**Learn to build safety-critical systems in C.**

Not "Hello World". Real kernels. Mathematical rigour. Zero dependencies.

> *"Don't learn to code. Learn to prove, then transcribe."*

## Philosophy

Most tutorials teach you to write code that *seems* to work.  
This course teaches you to write code that *provably* works.

**The method:**
1. Define the problem precisely
2. Prove correctness mathematically  
3. Design structs that embody the proof
4. Code (it writes itself)
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

## Projects

### [Module 1: Pulse](./projects/pulse/) — Heartbeat Liveness Monitor

*"Does something exist in time?"*

A tiny, provably-correct state machine that answers: *"Is this process alive?"*

- 5 lessons from problem definition to hardened code
- ~200 lines of C with mathematical proofs
- Handles clock wrap, faults, and edge cases
- Zero dependencies beyond the Standard C library (libc)

**Contracts proven:** Soundness, Liveness, Stability

---

### [Module 2: Baseline](./projects/baseline/) — Statistical Normality Monitor

*"Is what's happening normal?"*

A closed, total, deterministic FSM for detecting statistical anomalies in scalar streams.

- 6 lessons from problem statement to composition
- EMA-based anomaly detection with O(1) memory
- 18 contract and invariant tests
- Composes with Pulse for timing anomaly detection

**Contracts proven:** Convergence, Sensitivity, Stability, Spike Resistance

---

### Coming Next: Module 3 — Composition

*Pulse + Baseline = Timing Anomaly Detector*

```
event_t → Pulse → Δt → Baseline → deviation?
```

## The Big Picture

| Module | Question | Output |
|--------|----------|--------|
| Pulse | Does something happen? | Yes / No |
| Baseline | Is what's happening normal? | Degree of deviation |
| Composition | Is the system healthy? | Proof-carrying signal |

Each module is:
- **Closed** — State depends only on previous state + input
- **Total** — Always returns a valid result
- **Bounded** — O(1) memory, O(1) compute
- **Deterministic** — Same inputs → same outputs
- **Contract-defined** — Behaviour is specified, not implied

## Who This Is For

- Developers who want to understand *why* code works, not just *that* it works
- Systems programmers building safety-critical software
- Anyone tired of "it works on my machine" engineering
- Students who want to learn C the right way

## Prerequisites

- Basic C syntax (variables, functions, structs)
- Comfort with command line
- Willingness to think before coding

## Getting Started

```bash
# Module 1: Pulse
cd projects/pulse
make
make demo
make test

# Module 2: Baseline
cd projects/baseline
make
make demo
make test
```

Then start with [Pulse Lesson 1: The Problem](./projects/pulse/lessons/01-the-problem/LESSON.md).

## Author

**William Murray** — 30 years UNIX systems engineering

- GitHub: [@williamofai](https://github.com/williamofai)
- LinkedIn: [William Murray](https://www.linkedin.com/in/williammurray1967/)
- Website: [speytech.com](https://speytech.com)

## License

MIT — See [LICENSE](./LICENSE)
