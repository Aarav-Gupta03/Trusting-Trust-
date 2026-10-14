# Contributing to Trusting Trust, Revisited

Thank you for your interest in contributing to this research project! We welcome improvements to the compiler pipeline, additional detection mechanisms, performance benchmarks, and formal verification proofs.

---

## Code of Conduct & Safety Principles

1. **Defanged Payloads Only**:
   All attack demonstrations must remain strictly safe and inert. Pull requests attempting to introduce real authentication bypasses, malicious shellcode, network backdoors, or weaponized payloads will be closed immediately.

2. **Determinism & Reproducibility**:
   Every phase must compile cleanly with standard C++17 (`g++ -std=c++17 -Wall -Wextra -O2`) and standard Python 3 with zero external library dependencies.

---

## Development & Testing Workflow

This project is designed to be built in Linux, POSIX environments, or Windows Subsystem for Linux (WSL).

### 1. Prerequisites
- **GCC / G++**: Supporting C++17 (g++ >= 9.0)
- **Make**: Standard GNU Make
- **Python**: 3.8+ (standard library only)
- **Git**: For version control and testing the Phase 4 replication cycle

### 2. Verification Steps Before Submitting
Before submitting changes, test the pipeline:

```bash
# Phase 1: Build honest compiler and run test suite
cd 01_phase1_clean_compiler && make test && cd ..

# Phase 2: Verify targeted marker injection
cd 02_phase2_marker_injection && make all && ./check_injection.sh && cd ..

# Phase 3: Verify exact quine fixed-point
cd 03_phase3_quine && g++ -std=c++17 quine.cpp -o quine && ./quine | diff - quine.cpp && cd ..

# Phase 4: Run full git-based self-replication attack
cd 04_phase4_self_replication && ./demonstrate_attack.sh && cd ..

# Phase 5: Run Wheeler's Diverse Double-Compiling verification harness
cd 05_phase5_ddc_defense && ./ddc_verify.sh --clean-only && ./ddc_verify.sh && cd ..

# Phase 6: Run extension tests and detection matrix
cd 06_phase6_extensions/tools && python3 run_detection_matrix.py && cd ../..
```

### 3. Commit Guidelines
- Use clear, descriptive commit messages (e.g., `feat(phase6): add metamorphic relation for variable renaming`, `docs: update formal proof walkthrough`).
- Ensure no object files (`*.o`), binaries (`tlc`, `quine`), or generated test outputs are committed.
