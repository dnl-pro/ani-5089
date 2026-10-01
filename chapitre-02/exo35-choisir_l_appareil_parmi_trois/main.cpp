#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

struct Appareil {
    std::string serie;
    std::string etat;
    std::string modele;
};

int main() {
    int D;
    std::cin >> D;

    std::vector<Appareil> appareils;

    for (int i = 0; i < D; ++i) {
        Appareil appareil;
        std::cin >> appareil.serie >> appareil.etat >> appareil.modele;
        appareils.push_back(appareil);
    }

    std::string cible;
    std::cin >> cible;

    if (cible != "-") {
        for (const Appareil& appareil : appareils) {
            if (appareil.serie == cible) {
                if (appareil.etat != "device") {
                    std::cout << "ERREUR " << appareil.serie
                              << " est " << appareil.etat << '\n';
                } else {
                    std::cout << appareil.serie << '\n';
                }
                return 0;
            }
        }

        std::cout << "ERREUR cible introuvable\n";
        return 0;
    }

    std::vector<std::string> disponibles;

    for (const Appareil& appareil : appareils) {
        if (appareil.etat == "device") {
            disponibles.push_back(appareil.serie);
        }
    }

    if (disponibles.empty()) {
        std::cout << "ERREUR aucun appareil\n";
        return 0;
    }

    if (disponibles.size() == 1) {
        std::cout << disponibles[0] << '\n';
        return 0;
    }

    std::sort(disponibles.begin(), disponibles.end());

    std::cout << "ERREUR plusieurs appareils\n";

    for (const std::string& serie : disponibles) {
        std::cout << serie << '\n';
    }

    return 0;
}
