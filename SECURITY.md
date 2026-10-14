# Security Policy & Ethical Disclosure

## Pedagogical & Defensive Scope

This repository provides a rigorous, research-oriented reference implementation of **Ken Thompson's classic 1984 Turing Award lecture: *"Reflections on Trusting Trust"*** and evaluates modern verification countermeasures, including **David A. Wheeler's Diverse Double-Compiling (DDC)** and **Metamorphic Testing**.

### Intentional Safety Design (Defanged Payloads)

To adhere strictly to computer science ethics and educational safety standards:
1. **Inert Payloads Only**: Every payload in every phase is deliberately inert — emitting labeled diagnostic comments (`// [MARKER]`) or diagnostic stderr strings.
2. **Zero Functional Bypass**: There are no functional credential bypasses, hidden passwords, remote backdoors, or exploit code in this repository.
3. **Focus on Compiler Mechanics**: The engineering focus is entirely on the theoretical and mechanical aspects of the attack:
   - AST node detection
   - Quine-driven self-replication
   - Attaining a stable fixed point across compiler generations
   - Verification and divergence detection through independent static, behavioral, and DDC defense mechanisms.

## Responsible Disclosure

If you identify an unintentional vulnerability or unintended side effect within the tooling or scripts:
- Please open a private security advisory or contact the repository maintainer directly at: `aaravguptagrd5@gmail.com`.
- Please do not submit PRs introducing weaponized payloads or functional authentication bypass mechanisms.
