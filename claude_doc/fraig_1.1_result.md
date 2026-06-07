# fraig Step 1.1 — Single Command Quick Verification Result

**Date:** 2026-06-02
**Goal:** Run `strash; fraig; ps` against `strash; ps` on the 6 stubborn cases that v1z fails to optimize, and check whether fraig alone has any effect.

## Commands

Noopt baseline:

```
read_blif <case>/input.blif; strash; ps
```

fraig test:

```
read_blif <case>/input.blif; strash; fraig; ps
```

## Results

| Case      | I/O     | noopt AND | fraig AND | noopt Level | fraig Level | Delta AND |
|-----------|---------|-----------|-----------|-------------|-------------|-----------|
| tc_public_7  | 130/63  | 252       | 252       | 3           | 3           | 0         |
| tc_public_9  |  67/32  | 224       | 224       | 5           | 5           | 0         |
| tc_public_16 |   8/5   |  45       |  45       | 6           | 6           | 0         |
| tc_public_17 |  16/6   |  93       |  93       | 6           | 6           | 0         |
| tc_public_23 | 520/2   | 1530      | 1530      | 263         | 263         | 0         |
| tc_public_26 |  33/32  | 1064      | 1064      | 32          | 32          | 0         |

## Conclusion

`fraig` has **zero effect** on all 6 stubborn cases. Both AND node count and level count are identical between noopt and fraig. This means after structural hashing (`strash`), these circuits contain no functionally equivalent nodes that `fraig` can merge.

Per the TODO decision tree, this is **Case B** (fraig is also ineffective on the stubborn cases).

## Next Steps (pending user decision)

- Option A: Still run step 1.2 (full 30-case comparison of v1z vs v1z+fraig) to check whether `fraig` has synergistic effects when embedded in the full optimization sequence.
- Option B: Skip to Case B analysis — use `print_stats` / `print_sharing` to analyze the structural characteristics of the 6 stubborn cases and discuss alternative directions.
