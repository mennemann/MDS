#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#define POP_SIZE 50

#ifdef DEBUG_BUILD
#define DEBUG_BLOCK(code) code;
#define RELEASE_BLOCK(code) ;
#else
#define DEBUG_BLOCK(code) ;
#define RELEASE_BLOCK(code) code;
#endif

// ############### Graph ###############

using Graph = std::vector<std::vector<uint32_t>>;

Graph read_gr_file(std::istream& in) {
    std::string line;
    uint32_t n, u, v;

    Graph adj;

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == 'c') continue;
        if (line[0] == 'p') {
            std::istringstream iss(line);
            std::string tmp;
            uint32_t m;

            iss >> tmp >> tmp >> n >> m;
            adj.resize(n);
        } else {
            std::istringstream iss(line);
            std::string tmp;
            iss >> u >> v;

            adj[u - 1].push_back(v - 1);
            adj[v - 1].push_back(u - 1);
        }
    }

    return adj;
}

void get_uncovered_vertices(const Graph& adj, const std::vector<bool>& dom_set, std::vector<uint32_t>& uncovered) {
    std::vector<bool> covered(adj.size(), false);

    for (uint32_t u = 0; u < adj.size(); ++u) {
        if (dom_set[u]) {
            covered[u] = true;
            for (uint32_t v : adj[u])
                covered[v] = true;
        }
    }

    uncovered.clear();
    for (uint32_t u = 0; u < adj.size(); ++u)
        if (!covered[u]) uncovered.push_back(u);
}

// ############### GA Methods ###############

struct Individual {
    std::vector<bool> dom_set;
    uint32_t fitness = 0;
};

void update_fitness(Individual& ind) {
    ind.fitness = std::count(ind.dom_set.begin(), ind.dom_set.end(), true);
}

const Individual& tournament_select(const std::vector<Individual>& pop, std::mt19937& rng, size_t k = 2) {
    std::uniform_int_distribution<> dist(0, pop.size() - 1);
    const Individual* best = nullptr;
    for (size_t i = 0; i < k; ++i) {
        const Individual& candidate = pop[dist(rng)];
        if (!best || candidate.fitness < best->fitness) {
            best = &candidate;
        }
    }

    return *best;
}

const Individual& random_select(const std::vector<Individual>& pop, std::mt19937& rng) {
    std::uniform_int_distribution<> dist(0, pop.size() - 1);

    return pop[dist(rng)];
}

auto best_select_it(std::vector<Individual>& pop) {
    return std::min_element(pop.begin(), pop.end(),
                            [](const Individual& a, const Individual& b) {
                                return a.fitness < b.fitness;
                            });
}

auto worst_select_it(std::vector<Individual>& pop) {
    return std::max_element(pop.begin(), pop.end(),
                            [](const Individual& a, const Individual& b) {
                                return a.fitness < b.fitness;
                            });
}

void full_repair(const Graph& adj, std::vector<bool>& dom_set) {
    std::vector<uint32_t> uncovered;
    get_uncovered_vertices(adj, dom_set, uncovered);

    for (auto u : uncovered) {
        dom_set[u] = true;
    }
}

void greedy_random_repair(const Graph& adj, std::vector<bool>& dom_set, std::mt19937& rng) {
    std::vector<uint32_t> uncovered;
    get_uncovered_vertices(adj, dom_set, uncovered);

    std::unordered_map<uint32_t, size_t> indexMap;
    for (size_t i = 0; i < uncovered.size(); i++) {
        indexMap[uncovered[i]] = i;
    }

    size_t idx;

    while (!uncovered.empty()) {
        std::uniform_int_distribution<> dis(0, uncovered.size() - 1);

        idx = dis(rng);

        uint32_t new_v = uncovered[idx];
        dom_set[new_v] = true;

        uncovered[idx] = uncovered.back();
        indexMap[uncovered[idx]] = idx;
        uncovered.pop_back();
        indexMap.erase(new_v);

        for (auto neigh : adj[new_v]) {
            auto it = indexMap.find(neigh);
            if (it == indexMap.end()) continue;

            idx = indexMap[neigh];

            uncovered[idx] = uncovered.back();
            indexMap[uncovered[idx]] = idx;
            uncovered.pop_back();
            indexMap.erase(neigh);
        }
    }
}

void random_mutate(std::vector<bool>& dom_set, std::mt19937& rng, double mutate_prob) {
    std::uniform_real_distribution<> prob(0.0, 1.0);

    for (size_t i = 0; i < dom_set.size(); i++) {
        if (prob(rng) < mutate_prob) {
            dom_set[i] = prob(rng) < 0.5;
        }
    }
}

void false_mutate(std::vector<bool>& dom_set, std::mt19937& rng, double mutate_prob) {
    std::uniform_real_distribution<> prob(0.0, 1.0);

    for (size_t i = 0; i < dom_set.size(); i++) {
        if (prob(rng) < mutate_prob) {
            dom_set[i] = false;
        }
    }
}

Individual uniform_crossover(const Individual& a, const Individual& b, std::mt19937& rng) {
    Individual child;
    child.dom_set.resize(a.dom_set.size());

    std::bernoulli_distribution dist(0.5);
    for (size_t i = 0; i < a.dom_set.size(); i++) {
        child.dom_set[i] = dist(rng) ? a.dom_set[i] : b.dom_set[i];
    }

    return child;
}

Individual set_intersection_crossover(const Individual& a, const Individual& b) {
    Individual child;
    child.dom_set.resize(a.dom_set.size());

    for (size_t i = 0; i < a.dom_set.size(); i++) {
        child.dom_set[i] = a.dom_set[i] && b.dom_set[i];
    }

    return child;
}

void replace_weakest(std::vector<Individual>& pop, const Individual& child) {
    auto weakest = worst_select_it(pop);

    if (child.fitness < weakest->fitness) {
        *weakest = child;
    }
}

// ############### main ###############

Individual best;

void signal_handler(int signum) {
    (void)signum;

    std::cout << std::count(best.dom_set.begin(), best.dom_set.end(), true) << std::endl;
    for (uint32_t j = 0; j < best.dom_set.size(); j++) {
        if (best.dom_set[j]) std::cout << j + 1 << std::endl;
    };

    std::exit(0);
}

int main() {
    std::signal(SIGTERM, signal_handler);

    std::random_device rd;
    std::mt19937 rng(rd());

    int i;

    DEBUG_BLOCK(std::cout << "Loading graph" << std::endl);

    const auto& adj = read_gr_file(std::cin);
    const uint32_t n = adj.size();

    i = 0;
    auto pop = std::vector<Individual>(POP_SIZE);
    for (auto& ind : pop) {
        ++i;
        DEBUG_BLOCK(std::cout << "Initializing population - " << i << "\r" << std::flush);

        ind.dom_set = std::vector<bool>(n, true);
        false_mutate(ind.dom_set, rng, 0.3);
        greedy_random_repair(adj, ind.dom_set, rng);
        update_fitness(ind);

        if (i == 1) best = ind;
    }

    DEBUG_BLOCK(std::cout << "Starting optimization" << std::endl);

    i = 0;
    while (true) {
        ++i;
        DEBUG_BLOCK(std::cout << i << " - " << best_select_it(pop)->fitness << std::endl);

        const Individual& parent = tournament_select(pop, rng);

        Individual child = parent;

        random_mutate(child.dom_set, rng, 0.01);
        greedy_random_repair(adj, child.dom_set, rng);
        update_fitness(child);

        replace_weakest(pop, child);

        best = *best_select_it(pop);
    }

    return 0;
}
