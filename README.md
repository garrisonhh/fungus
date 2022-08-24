# fungus

***I've taken the ideas and lessons from this project and started off in a new direction with a much more focused scope. check out [fluent](https://github.com/garrisonhh/fluent)! leaving this here for posterity, though!***

An organic language-oriented programming language.

## v1 checklist

- [X] compile-time lexemes (symbols and keywords)
- [X] unified compile-time and runtime type system
  - [X] algebraic types
- [X] compile-time operator precedence
- [X] compile-time syntax rule definitions for parsing tokens into an AST
  - [x] meta-compiling milestone: compile new syntax from fungus syntax
  - [ ] syntax rule ambiguity checking
  - [ ] strong static type analysis of AST
- [ ] IR generation from AST
  - [ ] IR compile-time evaluation
- [ ] C generation from IR

## getting started

Fungus is written in C11 and [zig](https://github.com/ziglang/zig), and also uses zig as its build system and compiler toolchain. Once zig is installed:

```
cd fungus
zig build
```

This will produce a `fungus` executable in the base directory of the project.

<a rel="license" href="http://creativecommons.org/licenses/by-nc-sa/4.0/"><img alt="Creative Commons License" style="border-width:0" src="https://i.creativecommons.org/l/by-nc-sa/4.0/88x31.png" /></a><br />This work is licensed under a <a rel="license" href="http://creativecommons.org/licenses/by-nc-sa/4.0/">Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License</a>.
