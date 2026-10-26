# Logix - Mini Language Compiler (Flex + Bison)

Compiler Construction Lab project, Metropolitan University, Bangladesh.
Compiles the fixed mini language from Section 5 of the project manual down to Three Address Code (TAC).

## Team
| Name | ID | GitHub |
|------|----|--------|
| Nadia Muntaha | _fill in_ | _fill in_ |
| Fahima Akter | | |

## Status
| Phase | Where | State |
|-------|-------|-------|
| Lexer (Flex) | `src/lexer/lexer.l` | done |
| Parser + CFG (Bison) | `src/parser/parser.y` | grammar done, error recovery via `error ';'` |
| AST | `src/ast/` | done (`--ast` prints the tree) |
| Symbol table (nested scopes) | `src/symtab/` | done (`--symtab` prints it) |
| Semantic analyzer | `src/semantic/` | done (6 error types) |
| TAC generator | `src/icg/` | done (printed to stdout) |

## Requirements
Linux, `gcc`, `flex`, `bison`, `make` (`sudo apt install gcc flex bison make`).

## Build
```bash
make          # builds ./logix
make clean
```

## Run
```bash
./logix examples/sample.mc              # prints Three Address Code to stdout
./logix examples/sample.mc > out.tac    # save the TAC
./logix examples/sample.mc --ast        # also print the AST
./logix examples/sample.mc --symtab     # also print the symbol table
```

## Test
```bash
make test     # runs valid (+ TAC comparison), lexical, syntax and semantic tests
```
Each `tests/valid/X.mc` has its expected TAC in `tests/valid/X.tac`.
Semantic-error tests in `tests/semantic_errors/` are enabled by the empty file `tests/.semantic_ready`.

## Layout
```
docs/        report outline + formal grammar
src/         lexer/ parser/ ast/ semantic/ symtab/ icg/ + main.c
tests/       valid, lexical_errors, syntax_errors, semantic_errors
examples/    sample programs
prototypes/  earlier hand-written C / C++ / Java experiments (not part of the graded build)
```
