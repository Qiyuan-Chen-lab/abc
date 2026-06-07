# Contest Reference

## Problem

The contest task is multi-output logic cone logic sharing optimization on BLIF netlists using ABC.

The central opportunity is that traditional local logic optimization often works inside single-output windows, while real designs can contain meaningful shared logic across multiple output cones.

## Inputs

- Public and hidden BLIF test cases.
- All cases must be readable by ABC.

## Validity Requirements

Each evaluated case must satisfy:

- No crash.
- Optimized output passes combinational equivalence checking with ABC `cec`.

If a case fails either condition, it receives zero score.

## Scoring Metrics

| Metric | Weight | Direction |
| --- | ---: | --- |
| AIG node count | 50% | Lower is better |
| AIG level count | 20% | Lower is better |
| Runtime | 20% | Lower is better |
| Peak memory | 10% | Lower is better |

Evaluation is single-threaded.

## Practical Priority Order

1. Preserve correctness and robustness.
2. Reduce AIG node count.
3. Avoid unacceptable AIG level growth.
4. Keep runtime practical on all public cases.
5. Avoid memory-heavy algorithms unless the node-count gain is clear.

## Deliverables Context

Expected contest deliverables include:

- Algorithm design document.
- Compiled ABC-based executable.
- Optimized BLIF files for test cases.

