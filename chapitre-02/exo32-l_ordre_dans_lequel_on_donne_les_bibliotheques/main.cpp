#include <algorithm>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>

int main() {
    int N;
    std::cin >> N;

    std::map<std::string, std::vector<std::string>> dependances;

    for (int i = 0; i < N; ++i) {
        std::string module;
        std::cin >> module;

        std::string ligne;
        std::getline(std::cin, ligne);

        std::istringstream iss(ligne);
        std::string besoin;

        while (iss >> besoin) {
            dependances[module].push_back(besoin);
        }
    }

    int M;
    std::cin >> M;

    std::set<std::string> modules;

    for (int i = 0; i < M; ++i) {
        std::string module;
        std::cin >> module;
        modules.insert(module);
    }

    // Construire la liste complète des modules nécessaires.
    std::vector<std::string> a_traiter(modules.begin(), modules.end());

    for (std::size_t i = 0; i < a_traiter.size(); ++i) {
        const std::string& module = a_traiter[i];

        auto it = dependances.find(module);
        if (it == dependances.end()) {
            continue;
        }

        for (const std::string& besoin : it->second) {
            if (modules.insert(besoin).second) {
                a_traiter.push_back(besoin);
            }
        }
    }

    // Compter combien de modules dépendent de chaque module.
    std::map<std::string, int> degre;

    for (const std::string& module : modules) {
        degre[module] = 0;
    }

    for (const std::string& module : modules) {
        auto it = dependances.find(module);
        if (it == dependances.end()) {
            continue;
        }

        for (const std::string& besoin : it->second) {
            if (modules.count(besoin)) {
                ++degre[besoin];
            }
        }
    }

    // Les candidats à zéro sont toujours pris par ordre alphabétique.
    std::set<std::string> disponibles;

    for (const auto& [module, d] : degre) {
        if (d == 0) {
            disponibles.insert(module);
        }
    }

    std::vector<std::string> resultat;

    while (!disponibles.empty()) {
        auto it_disponible = disponibles.begin();
        std::string module = *it_disponible;
        disponibles.erase(it_disponible);

        resultat.push_back(module);

        auto it = dependances.find(module);
        if (it == dependances.end()) {
            continue;
        }

        for (const std::string& besoin : it->second) {
            --degre[besoin];

            if (degre[besoin] == 0) {
                disponibles.insert(besoin);
            }
        }
    }

    if (resultat.size() != modules.size()) {
        std::cout << "CYCLE\n";
        return 0;
    }

    for (const std::string& module : resultat) {
        std::cout << module << '\n';
    }

    return 0;
}
