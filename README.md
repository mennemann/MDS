# Minimum Dominating Set

This repository is my submission for the heuristic track of the *PACE 2025 - Dominating Set* challenge. More information is available on the [PACE Challenge website](https://pacechallenge.org/2025).

## Solver description
The solver is written in **C++** and combines several heuristics, including greedy algorithms, local search and a genetic algorithm. *Details will follow soon.*

## Installation Guide

Requirements:
- C++17 compatible compiler (e.g. `g++`)
- CMake ≥ 3.10
- Make

Clone and build the solver:
```bash
git clone https://github.com/mennemann/MDS.git
cd MDS
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ../solver
make
```

This will produce the `mds` executeable in the `MDS/build/` directory.

## Usage

The solver reads a graph from `stdin` and outputs its currently best solution to `stdout` upon receiving a `SIGTERM` signal.
Details on the input and output format can be found at the [PACE 2025 website](https://pacechallenge.org/2025/ds/#input-and-output).

### Example
```bash
cat <graph.gr> | ./mds > output.sol
```

You can use the `timeout` command to automatically send `SIGTERM` after a specified duration:
```bash
cat <graph.gr> | timeout 5m ./mds > output.sol
```
