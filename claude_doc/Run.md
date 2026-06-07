# Build, Run, and Benchmark Reference

## Build

Default build:

```bash
make
```

Fallback build without readline:

```bash
make ABC_USE_NO_READLINE=1
```

Build static library:

```bash
make libabc.a
```

Build shared library:

```bash
make ABC_USE_PIC=1 libabc.so
```

## ABC Batch Mode

Inspect a BLIF after structural hashing:

```bash
./abc -c "read_blif input.blif; strash; print_stats"
```

Run an optimization flow and write output:

```bash
./abc -c "read_blif input.blif; strash; <optimization-flow>; print_stats; write_blif output.blif"
```

Check combinational equivalence:

```bash
./abc -c "cec original.blif optimized.blif"
```

Measure runtime and peak memory:

```bash
/usr/bin/time -v ./abc -c "read_blif input.blif; strash; <optimization-flow>; write_blif output.blif"
```

## Reference Flows

The contest has no official baseline. These flows are internal references used
to compare candidate algorithms.

Strash-only reference:

```bash
./abc -c "read_blif input.blif; strash; print_stats"
```

ABC standard reference:

```bash
./abc -c "read_blif input.blif; strash; b; rewrite; refactor; b; rewrite -lz; b; refactor -z; resub; b; rewrite -lz; refactor -z; b; print_stats; write_blif output.blif"
./abc -c "cec input.blif output.blif"
```

## Common ABC Commands

I/O:

- `read_blif`, `r`: read BLIF.
- `write_blif`, `w`: write BLIF.
- `read`, `write`: generic read/write commands.

Queries:

- `print_stats`, `ps`: print node count, levels, and I/O count.
- `print_io`: print I/O information.
- `print_level`: print level information.
- `print_supp`: print support information.
- `print_factor`: print factoring information.

Optimization:

- `strash`: structurally hash into AIG.
- `balance`, `b`: tree balancing.
- `rewrite`: DAG-aware AIG rewriting.
- `rewrite -l`: level-aware rewriting.
- `rewrite -z`: zero-cost rewriting.
- `refactor`: AIG refactoring.
- `refactor -z`: zero-cost refactoring.
- `resub`: resubstitution.
- `&mfs`: MFS optimization.
- `&resyn2`: standard optimization script.

Verification:

- `cec`: combinational equivalence checking.

## Moshare Contest Commands

The `moshare` command is the unified entry point for contest multi-output sharing algorithms.

List available algorithms:

```bash
./abc -c "moshare -algo list"
```

Run no-op safety reference (no change to network):

```bash
./abc -c "read_blif input.blif; strash; moshare -algo none -stats"
```

Run a specific algorithm:

```bash
./abc -c "read_blif input.blif; strash; moshare -algo <name> -maxwin <n> -maxcut <n> -maxcand <n> -stats"
```

Full smoke test flow:

```bash
./abc -c "read_blif input.blif; strash; print_stats; moshare -algo <name> -stats; print_stats; write_blif claude_run/moshare_<name>_case.blif"
./abc -c "cec input.blif claude_run/moshare_<name>_case.blif"
```

See `claude_doc/moshare_integration_guide.md` for all flags and defaults.

## Benchmark Discipline

For each benchmarked case, record:

- Exact command line.
- Original AIG node count and level after `strash`.
- Optimized AIG node count and level.
- Runtime.
- Peak memory when measured.
- CEC result.

Store durable benchmark summaries, logs, and reports under `claude_doc/`.
Use `claude_run/` for temporary generated BLIF files and other run artifacts.
