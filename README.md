# fungus

An organic language-oriented programming language focused on creating and using embedded DSLs to write truly zero-overhead abstractions.

## status

Feature goals for the first version:

- [X] compile-time lexemes (symbols and keywords)
- [X] unified compile-time and runtime type system
  - [X] algebraic types
- [X] compile-time operator precedence
- [X] compile-time syntax rule definitions for parsing tokens into an AST
  - [ ] syntax rule ambiguity checking
  - [ ] strong static type analysis of AST
- [ ] IR generation from AST
  - [ ] IR compile-time evaluation
- [ ] C generation from IR

## getting started

Fungus is written in C11, and uses [zig](https://github.com/ziglang/zig) as its build system and compiler toolchain. Once zig is installed and available in your shell, simply:

```
cd fungus
zig build
```

This will produce a `fungus` executable in the base directory of the project.

*If zig is unavailable or hard to get on your system, (cough, windows...) simply compiling all of the C source files in src/ using your favorite (reasonably modern) compiler will work just fine.*

## using

One of the key ideas of fungus is compile-time syntax description. Because of this, fungus rules can be inspected from within the repl itself, as organic, living self-documentation. Running `fungus` will show all of the current definitions.

Fungus is heavily WIP currently, until the design has been solidified and documentation that won't go stale within a day or two can be written, the best course of entry is to read the code.

<a rel="license" href="http://creativecommons.org/licenses/by-nc-sa/4.0/"><img alt="Creative Commons License" style="border-width:0" src="https://i.creativecommons.org/l/by-nc-sa/4.0/88x31.png" /></a><br />This work is licensed under a <a rel="license" href="http://creativecommons.org/licenses/by-nc-sa/4.0/">Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License</a>.
