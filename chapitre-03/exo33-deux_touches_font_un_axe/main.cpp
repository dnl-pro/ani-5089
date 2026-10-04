#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

struct Commande {
    std::string nom;
    int echelle;
    int seuil;
};

int main() {
    int c;
    std::cin >> c;

    std::vector<Commande> commandes(c);

    for (int i = 0; i < c; ++i) {
        std::cin >> commandes[i].nom
                 >> commandes[i].echelle
                 >> commandes[i].seuil;
    }

    int t;
    std::cin >> t;

    for (int tour = 0; tour < t; ++tour) {
        int axe = 0;

        for (int i = 0; i < c; ++i) {
            int brute;
            std::cin >> brute;

            int contribution = brute * commandes[i].echelle / 1000;

            if (std::abs(contribution) >= commandes[i].seuil) {
                axe += contribution;
            }
        }

        std::cout << axe << '\n';
    }

    return 0;
}
