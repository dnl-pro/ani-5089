#include <algorithm>
#include <iostream>
#include <map>
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

    std::set<std::string> resultat;

    for (int i = 0; i < M; ++i) {
        std::string module;
        std::cin >> module;
        resultat.insert(module);
    }

    std::vector<std::string> a_traiter(resultat.begin(), resultat.end());

    for (std::size_t i = 0; i < a_traiter.size(); ++i) {
        const std::string& module = a_traiter[i];

        auto it = dependances.find(module);
        if (it == dependances.end()) {
            continue;
        }

        for (const std::string& besoin : it->second) {
            if (resultat.insert(besoin).second) {
                a_traiter.push_back(besoin);
            }
        }
    }

    for (const std::string& module : resultat) {
        std::cout << module << '\n';
    }

    return 0;
}
