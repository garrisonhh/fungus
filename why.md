# What is this?

My language Mycelium's goal is to have 'organic syntax' that grows with a
program. The idea is that EVERY language allows syntax rewriting to some extent,
even it is just in the form of creating custom types and writing procedures.
Some languages completely resist any idea of true metaprogramming (syntax
modification), saying it is 'vague for outside programmers'.

Mycelium says, fuck that. If you replaced the entire Go or Python standard
library, even without changing a single thing within the language, would
experienced Go or Python programmers understand what the fuck is going on?
Absolutely not! Code itself, and the functions and variable names within it,
already has deeply contextual semantics.

Instead of nerfing redefinition of syntax as much as possible, Mycelium says,
'go forth and fuck it up'. Mycelium embraces the biology of a codebase, the
living organism that it embodies.

A common piece of advice shared within the programming community about languages
and libraries is something like 'use the tool that fits the job', asserting that
every language already has a purpose it is best suited for. Mycelium can be that
tool, in every case... it just requires the right syntax.

# Ideas

SEE THIS: https://nshipster.com/swift-operators/ ABOUT OPERATOR DEFINITIONS

This project is my prototype of Mycelium syntax rewriting tech, in a much
simpler language. This will be a simple AST-walk interpreted language, that
becomes more powerful with syntax redefinitions. Maybe call it Fungus?

## Basic Behavior

```
>>> x := 3
3 :: int

>>> y := x * 2
9 :: int

>>> x = 4 # redefine
4 :: int
```

## Dynamic Operators

TODO
