#include <iostream>
#include <set>
#include <string>
#include <vector>

struct Prefixe {
    std::string prefixe;
    std::string module;
};

int main() {
    int P;
    std::cin >> P;
    std::cin.ignore();

    std::vector<Prefixe> prefixes;

    for (int i = 0; i < P; ++i) {
        std::string prefixe;
        std::string module;

        std::cin >> prefixe >> module;
        prefixes.push_back({prefixe, module});
    }

    int L;
    std::cin >> L;
    std::cin.ignore();

    std::set<std::string> modules;
    int inconnus = 0;

    const std::string marqueur = "undefined reference to '";

    for (int i = 0; i < L; ++i) {
        std::string ligne;
        std::getline(std::cin, ligne);

        std::size_t debut = ligne.find(marqueur);

        if (debut == std::string::npos) {
            continue;
        }

        debut += marqueur.size();
        std::size_t fin = ligne.find('\'', debut);

        if (fin == std::string::npos) {
            continue;
        }

        std::string symbole = ligne.substr(debut, fin - debut);

        const Prefixe* meilleur = nullptr;

        for (const Prefixe& p : prefixes) {
            if (symbole.rfind(p.prefixe, 0) == 0) {
                if (meilleur == nullptr ||
                    p.prefixe.size() > meilleur->prefixe.size()) {
                    meilleur = &p;
                }
            }
        }

        if (meilleur != nullptr) {
            modules.insert(meilleur->module);
        } else {
            ++inconnus;
        }
    }

    for (const std::string& module : modules) {
        std::cout << module << '\n';
    }

    if (inconnus > 0) {
        std::cout << "INCONNU " << inconnus << '\n';
    }

    return 0;
}
