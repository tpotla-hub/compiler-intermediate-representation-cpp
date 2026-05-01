# Compiler with Intermediate Representation in C++

A small compiler that parses a custom programming language, generates a 
linked-list intermediate representation, and interprets it for execution. 
Built as part of CSE340 at Arizona State University.

## Overview

This project implements a full compiler pipeline — from parsing source code 
to generating and executing an intermediate representation (IR). Rather than 
targeting assembly, the compiler generates a graph-based IR that is then 
executed by an interpreter.

## Features

- **Recursive-Descent Parser** — parses a custom C-like language from scratch
- **IR Code Generation** — converts parsed statements into a linked list of 
  instruction nodes
- **Instruction Types Supported:**
  - `ASSIGN` — arithmetic expressions with `+`, `-`, `*`, `/`
  - `IN` / `OUT` — input and output statements
  - `CJMP` — conditional jump for `if` and `while` logic
  - `JMP` — unconditional jump for loop back edges and switch fall-through
  - `NOOP` — no-operation node used as branch targets
- **Control Flow Structures:**
  - `if` statements
  - `while` loops
  - `for` loops (desugared to while)
  - `switch` statements with optional `default` case
- **Interpreter** — executes the generated IR graph by traversing instruction 
  nodes and updating memory

## Tech Stack

![C++](https://img.shields.io/badge/C++-00599C?style=flat&logo=cplusplus&logoColor=white)
![GCC](https://img.shields.io/badge/GCC-A8B9CC?style=flat&logo=gnu&logoColor=black)
![Ubuntu](https://img.shields.io/badge/Ubuntu_22.04-E95420?style=flat&logo=ubuntu&logoColor=white)

## How to Compile & Run

```bash
# Compile
g++ -std=c++11 compiler.cpp execute.cc lexer.cpp -o a.out

# Run with an input program
./a.out < input_program.txt
```

## Example

**Input program:**
```
a, b, c;
{
  input a;
  input b;
  c = a + b;
  output c;
}
3 5
```

**Output:**
```
8
```

## How It Works

1. The parser reads the source program and allocates memory locations for 
   all declared variables
2. Each statement is compiled into one or more `InstructionNode` structs 
   linked together into a graph
3. Control flow (if, while, for, switch) is handled using `CJMP` and `JMP` 
   nodes that wire up branch targets at compile time
4. After the IR is fully built, the provided interpreter traverses the graph 
   and executes each instruction

## What I Learned

- How compilers translate high-level control flow into jump-based IR
- Designing and traversing graph-based data structures in C++
- The difference between compile-time IR generation and runtime execution
- Implementing a recursive-descent parser for a real language with 
  expressions, conditions, and nested control flow
