#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#define POP_SIZE 30

using Graph = std::vector<std::vector<uint32_t>>;

struct Individual {
    std::vector<bool> dom_set;
    uint32_t fitness = 0;
};

std::atomic<bool> sigterm_recv(false);

void sigterm_handler(int signum) {
    std::cout << "Recieved SIGTERM" << std::endl;
    sigterm_recv.store(true);
}

Graph read_gr_file(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    uint32_t n, u, v;

    Graph adj;

    while (std::getline(file, line)) {
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

void repair(const Graph& adj, std::vector<bool>& dom_set) {
    auto uncovered = std::vector<uint32_t>();
    get_uncovered_vertices(adj, dom_set, uncovered);

    for (auto i : uncovered) {
        dom_set[i] = true;
    }
}

void update_fitness(const Graph& adj, Individual& ind, std::vector<uint32_t>& uncovered) {
    get_uncovered_vertices(adj, ind.dom_set, uncovered);

    ind.fitness = std::count(ind.dom_set.begin(), ind.dom_set.end(), true) + uncovered.size();
}

void mutate(std::vector<bool>& dom_set, std::mt19937& rng, double flip_prob = 0.01) {
    std::uniform_real_distribution<> prob(0.0, 1.0);

    for (size_t i = 0; i < dom_set.size(); i++) {
        if (prob(rng) < flip_prob) {
            dom_set[i] = !dom_set[i];
        }
    }
}

const Individual& tournament_select(const std::vector<Individual>& pop, std::mt19937& rng, size_t k = 3) {
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

const Individual& best_select(const std::vector<Individual>& pop) {
    auto best = std::min_element(pop.begin(), pop.end(),
                                  [](const Individual& a, const Individual& b) {
                                      return a.fitness < b.fitness;
                                  });
    return *best;
}


void replace_weakest(std::vector<Individual>& pop, const Individual& child) {
    auto worst = std::max_element(pop.begin(), pop.end(),
                                  [](const Individual& a, const Individual& b) {
                                      return a.fitness < b.fitness;
                                  });

    if (child.fitness < worst->fitness) {
        *worst = child;
    }
}


int main(int argc, char* argv[]) {
    std::signal(SIGTERM, sigterm_handler);

    std::random_device rd;
    std::mt19937 rng(rd());

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <graph.gr>\n";
        return 1;
    }
    std::string filepath = argv[1];

    const auto& adj = read_gr_file(filepath);
    const uint32_t n = adj.size();

    std::cout << "Loaded graph" << std::endl;

    std::vector<uint32_t> uncovered;

    auto pop = std::vector<Individual>(POP_SIZE);
    for (auto& ind : pop) {
        ind.dom_set = std::vector<bool>(n, true);
        mutate(ind.dom_set, rng);
        update_fitness(adj, ind, uncovered);
    }

    std::cout << "Initialized population" << std::endl;


    int iteration = 0;
    while (!sigterm_recv.load()) {
        if (++iteration%10 == 0) std::cout << iteration << " - " << best_select(pop).fitness << std::endl;
        const Individual& parent = tournament_select(pop, rng);

        Individual child = parent;

        mutate(child.dom_set, rng);
        update_fitness(adj, child, uncovered);

        replace_weakest(pop, child);
    }

    auto best = best_select(pop);
    std::cout << best.fitness << std::endl;

    repair(adj, best.dom_set);


    return 0;
}
