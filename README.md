# Tail

**A minimal programming language with reversed function-call syntax.**

Tail turns conventional function calls inside out. Instead of `print("hello world")`,
you write `hello world("print")`. The arguments come first; the function name comes
last, inside parentheses. The idea extends naturally to multiple arguments and to
chaining.

```tail
hello world("print")            # -> print("hello world")
5("square")                     # -> square(5)
10, 20("add")                   # -> add(10, 20)
"hello", "world"("concat")      # -> concat("hello", "world")
"hello world"("upper")("print") # -> print(upper("hello world"))
```

Tail is written **entirely from scratch in C11** — a hand-written lexer, parser, and
tree-walking interpreter — with **zero external dependencies** beyond the C standard
library. It compiles to a single native binary.

---

## Features

- **Reversed function-call syntax** — arguments precede the function name.
- **Chaining** — the result of each call feeds the next: `2("square")("square")` → `16`.
- **Multiple arguments** — comma-separated: `10, 20("add")` → `30`.
- **Bare strings** — unquoted text is a string literal: `hello world("print")`.
- **Quoted strings** — with escape sequences: `"line1\nline2"("print")`.
- **Numbers** — integer and floating point.
- **Built-in functions** — `print`, `square`, `add`, `concat`, `upper`.
- **Zero dependencies** — only the C standard library; compiles with any C11 compiler.
- **Native binary** — no runtime or interpreter framework required.

---

## Quick Start

### Prerequisites

- A C11 compiler (`cc`, `clang`, or `gcc`)
- `make`

### Build

```sh
make
```

This produces the `tail` executable in the project root.

### Run

```sh
./tail examples.tail
```

Or pipe source through stdin:

```sh
echo 'hello world("print")' | ./tail
```

---

## Language Guide

### Reversed function calls

In Tail, a function call is written as:

```
<arguments> ( "functionName" )
```

which is equivalent to the conventional `functionName(<arguments>)`.

| Tail                                   | Conventional equivalent |
| -------------------------------------- | ----------------------- |
| `hello world("print")`                 | `print("hello world")`  |
| `5("square")`                          | `square(5)`             |
| `10, 20("add")`                        | `add(10, 20)`           |
| `"hello", "world"("concat")`           | `concat("hello", "world")` |

### Chaining

Multiple calls can be chained. The result of each call becomes the single argument
of the next call in the chain:

```tail
"hello world"("upper")("print")
```

conceptually evaluates as:

```text
print(upper("hello world"))
```

Chaining works with numbers too:

```tail
2("square")("square")   # -> 16
3, 4("add")("square")   # -> square(add(3, 4)) -> 49
```

### Values

- **Bare strings** — any unquoted text before a call is a string literal:
  `hello world("print")` passes the string `hello world`.
- **Quoted strings** — use double quotes for explicit strings, including escape
  sequences (`\n`, `\t`, `\"`, `\\`, `\r`, `\0`):
  `"hello", "world"("concat")`.
- **Numbers** — integer or decimal: `5`, `3.14`.

### Comments

Lines beginning with `#` are comments:

```tail
# This is a comment.
hello world("print")
```

### Program output

Every top-level expression's result is printed to stdout, one per line. The `print`
function is an identity that returns its argument unchanged, making intent explicit
and usable inside chains.

---

## Built-in Functions

| Function  | Signature            | Description                                  | Example                  | Result      |
| --------- | -------------------- | -------------------------------------------- | ------------------------ | ----------- |
| `print`   | `print(x)`           | Returns its argument unchanged (identity).   | `"hi"("print")`          | `hi`        |
| `square`  | `square(n)`          | Returns `n * n`.                             | `5("square")`            | `25`        |
| `add`     | `add(a, b)`          | Returns `a + b` (numbers).                   | `10, 20("add")`          | `30`        |
| `concat`  | `concat(a, b)`       | Returns the concatenation of two strings.    | `"a", "b"("concat")`     | `ab`        |
| `upper`   | `upper(s)`           | Returns an uppercase copy of `s`.            | `"hi"("upper")`          | `HI`        |

---

## Architecture

Tail follows the classic interpreter pipeline:

```text
Source Code
    ↓
Lexer
    ↓
Tokens
    ↓
Parser
    ↓
AST
    ↓
Interpreter
    ↓
Output
```

| Stage        | File             | Responsibility                                            |
| ------------ | ---------------- | --------------------------------------------------------- |
| **Lexer**    | `lexer.c`        | Converts source text into a stream of tokens.             |
| **Parser**   | `parser.c`       | Builds an abstract syntax tree (AST) from the tokens.     |
| **AST**      | `tail.h`         | Defines the `CallNode` / `Chain` structures.              |
| **Interpreter** | `interpreter.c` | Walks the AST, resolves built-ins, and produces output.   |
| **Values**   | `value.c`        | The dynamic `Value` type (number / string).               |
| **CLI**      | `main.c`         | Reads source from a file or stdin and drives the pipeline.|

---

## Building from Source

### Targets

| Target      | Description                                                       |
| ----------- | ----------------------------------------------------------------- |
| `make`      | Build the release binary (`tail`).                                |
| `make test` | Build (if needed) and run the test suite.                         |
| `make dev`  | Build with AddressSanitizer and UndefinedBehaviorSanitizer.       |
| `make clean`| Remove build artifacts.                                           |

The default build uses `-std=c11 -Wall -Wextra -Wpedantic -Werror` and `-O2`.

### Development build

```sh
make dev
```

This rebuilds with `-fsanitize=address,undefined` to catch memory errors and
undefined behavior during development.

---

## Testing

The test suite is a dependency-free shell harness. Each `.tail` program in
`test/cases/` has a matching `.out` file containing its expected output.

```sh
make test
```

The harness runs every case and diffs actual output against expected, reporting a
pass/fail summary. It exits non-zero if any test fails.

---

## CLI Usage

```
usage: ./tail [file.tail]
```

| Invocation            | Behavior                                        |
| --------------------- | ----------------------------------------------- |
| `./tail file.tail`    | Runs the source file and prints results.        |
| `./tail < file.tail`  | Reads source from standard input.               |
| `./tail`              | Reads source from standard input (no file).     |

### Exit codes

| Code | Meaning                                  |
| ---- | ---------------------------------------- |
| `0`  | Success.                                 |
| `1`  | Lex, parse, or runtime error.            |
| `2`  | Invalid command-line usage.              |

Errors are reported to stderr with the offending location (line/column) where
available.

---

## Project Structure

```
tail/
├── README.md            # this file
├── LICENSE              # MIT license
├── Makefile             # build, test, and dev targets
├── tail.h               # shared types and public API
├── lexer.c              # tokenizer
├── parser.c             # recursive-descent parser
├── value.c              # dynamic Value type
├── interpreter.c        # tree-walking interpreter + built-ins
├── main.c               # command-line interface
├── examples.tail        # sample programs
└── test/
    ├── run_tests.sh     # test harness
    └── cases/           # .tail programs + expected .out files
```

---

## License

Tail is released under the [MIT License](LICENSE).

Copyright (c) 2026 Tail contributors
