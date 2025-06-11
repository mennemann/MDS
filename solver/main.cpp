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

void greedy_priority_bucket_repair(const Graph& adj, std::vector<bool>& dom_set, std::mt19937& rng) {
    const uint32_t n = adj.size();
    std::vector<bool> covered(n, false);
    std::vector<int> gain(n);

    for (uint32_t v = 0; v < n; ++v) {
        if (dom_set[v]) {
            covered[v] = true;
            for (uint32_t neigh : adj[v]) {
                covered[neigh] = true;
            }
        }
    }

    size_t max_deg = 0;
    for (uint32_t v = 0; v < n; ++v) {
        max_deg = std::max(max_deg, adj[v].size());

        int g = 1;
        for (uint32_t neigh : adj[v])
            if (!covered[neigh]) g++;
        gain[v] = g;
    }

    std::vector<std::vector<uint32_t>> buckets(max_deg + 2);
    std::vector<std::pair<int, size_t>> position(n);

    for (uint32_t v = 0; v < n; ++v) {
        int g = gain[v];
        buckets[g].push_back(v);
        position[v] = {g, buckets[g].size() - 1};
    }

    auto remove_from_bucket = [&](uint32_t v) {
        auto [g, idx] = position[v];
        std::vector<uint32_t>& bucket = buckets[g];
        int last = bucket.back();
        bucket[idx] = last;
        position[last] = {g, idx};
        bucket.pop_back();
    };

    auto update_gain = [&](uint32_t v, int new_gain) {
        if (gain[v] == new_gain) return;
        remove_from_bucket(v);
        buckets[new_gain].push_back(v);
        position[v] = {new_gain, buckets[new_gain].size() - 1};
        gain[v] = new_gain;
    };

    auto get_max_gain_bucket = [&]() -> std::vector<uint32_t>& {
        for (int g = max_deg + 1; g >= 0; g--) {
            if (!buckets[g].empty()) return buckets[g];
        }
        throw std::runtime_error("no bucket left");
    };

    uint32_t covered_count = std::count(covered.begin(), covered.end(), true);
    while (covered_count < n) {
        std::vector<uint32_t>& bucket = get_max_gain_bucket();

        std::uniform_int_distribution<int> dist(0, (int)bucket.size() - 1);
        uint32_t v = bucket[dist(rng)];
        remove_from_bucket(v);
        dom_set[v] = true;

        if (!covered[v]) {
            covered[v] = true;
            covered_count++;
        }

        for (uint32_t neigh : adj[v]) {
            if (!covered[neigh]) {
                covered[neigh] = true;
                covered_count++;

                for (uint32_t w : adj[neigh]) {
                    if (!covered[w]) {
                        update_gain(w, gain[w] - 1);
                    }
                }
            }
        }

        for (uint32_t neigh : adj[v]) {
            if (!covered[neigh]) {
                update_gain(neigh, gain[neigh] - 1);
            }
        }
    }
}

void greedy_local_removal(const Graph& adj, std::vector<bool>& dom_set, std::mt19937& rng) {
    const uint32_t n = dom_set.size();
    std::vector<int> coverage(n, 0);

    for (uint32_t u = 0; u < n; ++u) {
        if (dom_set[u]) {
            coverage[u]++;
            for (auto v : adj[u]) coverage[v]++;
        }
    }

    std::vector<uint32_t> candidates;
    for (uint32_t u = 0; u < n; ++u)
        if (dom_set[u]) candidates.push_back(u);

    std::shuffle(candidates.begin(), candidates.end(), rng);

    for (uint32_t u : candidates) {
        bool removable = true;

        if (coverage[u] <= 1) {
            removable = false;
        }

        for (uint32_t v : adj[u]) {
            if (coverage[v] <= 1) {
                removable = false;
                break;
            }
        }

        if (removable) {
            dom_set[u] = false;
            coverage[u]--;
            for (auto v : adj[u]) coverage[v]--;
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
