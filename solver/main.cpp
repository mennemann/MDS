#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#define POP_SIZE 20

#ifdef DEBUG_BUILD
#define DEBUG_BLOCK(code) code;
#define RELEASE_BLOCK(code) ;
#else
#define DEBUG_BLOCK(code) ;
#define RELEASE_BLOCK(code) code;
#endif

std::atomic<bool> sigterm_recv(false);

void sigterm_handler(int signum) {
    sigterm_recv.store(true);
}

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

const Individual& best_select(const std::vector<Individual>& pop) {
    auto best = std::min_element(pop.begin(), pop.end(),
                                 [](const Individual& a, const Individual& b) {
                                     return a.fitness < b.fitness;
                                 });
    return *best;
}

void full_repair(const Graph& adj, std::vector<bool>& dom_set) {
    auto uncovered = std::vector<uint32_t>();
    get_uncovered_vertices(adj, dom_set, uncovered);

    for (auto i : uncovered) {
        dom_set[i] = true;
    }
}

void greedy_random_repair(const Graph& adj, std::vector<bool>& dom_set, std::mt19937& rng) {
    std::vector<uint32_t> uncovered;
    get_uncovered_vertices(adj, dom_set, uncovered);
    std::unordered_set uncovered_s(uncovered.begin(), uncovered.end());

    while (!uncovered_s.empty()) {
        std::uniform_int_distribution<> dis(0, uncovered_s.size() - 1);
        auto it = uncovered_s.begin();
        std::advance(it, dis(rng));
        uint32_t new_v = *it;

        dom_set[new_v] = true;
        uncovered_s.erase(new_v);

        for (auto neigh : adj[new_v]) {
            uncovered_s.erase(neigh);
        }
    }
}

void mutate(std::vector<bool>& dom_set, std::mt19937& rng, double flip_prob) {
    std::uniform_real_distribution<> prob(0.0, 1.0);

    for (size_t i = 0; i < dom_set.size(); i++) {
        if (prob(rng) < flip_prob) {
            dom_set[i] = false;
        }
    }
}

void replace_weakest(std::vector<Individual>& pop, const Individual& child) {
    auto weakest = std::max_element(pop.begin(), pop.end(),
                                    [](const Individual& a, const Individual& b) {
                                        return a.fitness < b.fitness;
                                    });

    if (child.fitness < weakest->fitness) {
        *weakest = child;
    }
}

// ############### main ###############

int main() {
    std::signal(SIGTERM, sigterm_handler);

    std::random_device rd;
    std::mt19937 rng(rd());

    DEBUG_BLOCK(int i);

    DEBUG_BLOCK(std::cout << "Loading graph" << std::endl);

    const auto& adj = read_gr_file(std::cin);
    const uint32_t n = adj.size();

    DEBUG_BLOCK(i = 0);
    auto pop = std::vector<Individual>(POP_SIZE);
    for (auto& ind : pop) {
        DEBUG_BLOCK(std::cout << "Initializing population - " << ++i << "\r" << std::flush);
        ind.dom_set = std::vector<bool>(n, true);
        mutate(ind.dom_set, rng, 0.3);

        greedy_random_repair(adj, ind.dom_set, rng);
        update_fitness(ind);
    }

    DEBUG_BLOCK(std::cout << "Starting optimization" << std::endl);
    DEBUG_BLOCK(i = 0);

    while (!sigterm_recv.load()) {
        DEBUG_BLOCK(std::cout << ++i << " - " << best_select(pop).fitness << std::endl);

        const Individual& parent = tournament_select(pop, rng);

        Individual child = parent;

        mutate(child.dom_set, rng, 0.1);
        greedy_random_repair(adj, child.dom_set, rng);
        update_fitness(child);

        replace_weakest(pop, child);
    }

    DEBUG_BLOCK(std::cout << "Recieved SIGTERM" << std::endl);

    auto best = best_select(pop);

    RELEASE_BLOCK(
        std::cout << std::count(best.dom_set.begin(), best.dom_set.end(), true) << std::endl;
        for (uint32_t j = 0; j < n; j++) {
            if (best.dom_set[j]) std::cout << j + 1 << std::endl;
        });

    return 0;
}
