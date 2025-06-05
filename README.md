# GCD RoCC Accelerator Study

This repository contains a complete implementation of a GCD (Greatest Common Divisor) accelerator using the RoCC (Rocket Custom Coprocessor) interface in Chipyard.

## Repository Structure

```
gcd-rocc/
├── build.sbt                  # SBT build configuration
├── src/
│   └── main/
│       └── scala/
│           └── GCDRoCC.scala  # Main accelerator implementation
├── tests/
│   ├── gcd-rocc.c            # Basic functionality test
│   ├── gcd-benchmark.c       # Performance benchmark test
│   └── gcd-quick-benchmark.c # Quick benchmark test
└── docs/
    ├── complete-rocc-tutorial.md    # Complete RoCC tutorial
    ├── gcd-rocc-tutorial.md        # GCD-specific tutorial
    ├── implementation-notes.md      # Implementation details
    ├── performance-analysis.md      # Performance analysis results
    └── project-summary.md          # Project overview and summary
```

## Overview

This project demonstrates how to:

1. **Design a Custom Accelerator**: Implement a hardware GCD accelerator using Chisel
2. **Integrate with RoCC Interface**: Connect the accelerator to RISC-V via RoCC
3. **Test and Benchmark**: Verify functionality and measure performance
4. **Document the Process**: Provide comprehensive tutorials and analysis

## Key Features

- **Hardware GCD Implementation**: Uses Euclidean algorithm in hardware
- **RoCC Integration**: Fully integrated with Rocket Chip's custom coprocessor interface
- **Multiple Test Programs**: Includes basic tests and performance benchmarks
- **Comprehensive Documentation**: Step-by-step tutorials and implementation notes

## Getting Started

1. **Read the Documentation**:
   - Start with `docs/project-summary.md` for an overview
   - Follow `docs/gcd-rocc-tutorial.md` for the GCD-specific implementation
   - Use `docs/complete-rocc-tutorial.md` for a comprehensive RoCC guide

2. **Examine the Implementation**:
   - Review `src/main/scala/GCDRoCC.scala` for the hardware design
   - Check `tests/` directory for example usage

3. **Run Tests**:
   - `gcd-rocc.c`: Basic functionality verification
   - `gcd-benchmark.c`: Performance measurements
   - `gcd-quick-benchmark.c`: Quick performance test

## Performance Results

The accelerator demonstrates significant speedup compared to software implementations:
- Hardware accelerator: ~9 cycles per GCD operation
- Software implementation: ~191 cycles per GCD operation
- **Speedup: ~21.2x**

See `docs/performance-analysis.md` for detailed analysis.

## Educational Value

This project serves as an educational resource for:
- Understanding hardware/software co-design
- Learning the RoCC interface
- Implementing custom RISC-V accelerators
- Performance analysis and optimization

## Integration with Chipyard

This accelerator is designed to work within the Chipyard framework. For integration instructions, see the documentation in the `docs/` directory.

## License

This project follows the Chipyard licensing terms. See the parent Chipyard repository for details.