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

### [Pulse](./projects/pulse/) — Heartbeat Liveness Monitor
A tiny, provably-correct state machine that answers: *"Is this process alive?"*

- 5 lessons from problem definition to hardened code
- ~200 lines of C with mathematical proofs
- Handles clock wrap, faults, and edge cases
- Zero dependencies beyond libc

*More projects coming...*

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
cd projects/pulse
make
./build/pulse
```

Then start with [Lesson 1: The Problem](./projects/pulse/lessons/01-the-problem/LESSON.md).

## Author

**William Murray** — 30 years UNIX systems engineering

- GitHub: [@williamofai](https://github.com/williamofai)
- LinkedIn: [William Murray](https://www.linkedin.com/in/williammurray1967/)
- Website: [speytech.com](https://speytech.com)

## License

MIT — See [LICENSE](./LICENSE)
