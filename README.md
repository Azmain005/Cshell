# Custom Shell in C

This project is a simple UNIX-style command-line shell implemented in C. It supports basic command execution, I/O redirection, pipes, command history, and signal handling.

## Features

✅ **Basic Command Execution**  
✅ **Input Redirection (`<`)**  
✅ **Output Redirection (`>` and `>>`)**  
✅ **Pipes (`|`)**  
✅ **Multiple Commands (`;`) and Conditional Execution (`&&`)**  
✅ **Command History (`history` command)**  
✅ **Signal Handling for Ctrl+C (SIGINT)**

## Getting Started

### Prerequisites

- GCC (or any C compiler)
- UNIX-based OS (Linux, macOS, WSL for Windows)

### Compilation

Use `gcc` to compile the code:

```bash
gcc -o my_shell shell.c
