# Minimum Dominating Set

This repository is my submission for the heuristic track of the *PACE 2025 - Dominating Set* challenge. More information is available on the [PACE Challenge website](https://pacechallenge.org/2025).

## Solver description
The solver is written in C++ and combines a greedy algorithm, local search and a genetic algorithm in order to find good approximate solutions for the minimum dominating set problem.

The greedy algorithm is used to repair an existing subset of the vertices into a decent dominating set. The algorithm sorts all unmarked vertices based on how much coverage will be gained by adding them to the dominating set and chooses one with maximum gain. This is repeated until all vertices are covered.

The local search removes vertices from a dominating set while ensuring that all vertices stay covered. It determines for each vertex, by how many it is covered. A vertex is removed from the dominating set if the coverage of it and its neighbours stays above 0.

The genetic algorithm combines the previous algorithm in order to find even better solutions:
1. A population of 40 candidates (dominating sets) is initialized by using the greedy algorithm on an empty set and using the local search to removed unneccessary vertices.
2. The following steps are repeated until `SIGTERM` is recieved:
    1. Tournament selection chooses 2 candidates from the existing population (parents).
    2. A set intersection of the 2 parents is performed as a starting point for the new candidate.
    3. The greedy algorithm and local search are used to make the new candidate a valid dominating set.
    4. The weakest (largest) candidate from the population is replaced by the new one (if it is better).

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
