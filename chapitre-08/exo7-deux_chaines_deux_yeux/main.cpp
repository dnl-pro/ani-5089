#include <iostream>
#include <string>

struct Chaine {
    long long suivant = 0;
    bool emprunte = false;
    long long rendus = 0;
};

int main() {
    long long images = 0;
    long long n = 0;
    if (!(std::cin >> images >> n)) {
        return 0;
    }
    if (images < 1) {
        images = 1;
    }

    Chaine gauche;
    Chaine droite;
    long long desync = 0;
    long long difference = 0;

    for (long long tour = 1; tour <= n; ++tour) {
        std::string oeil;
        std::string commande;
        if (!(std::cin >> oeil >> commande)) {
            break;
        }
        Chaine& chaine = (oeil == "G") ? gauche : droite;

        std::cout << tour << ' ' << oeil << ' ' << commande << ' ';
        if (commande == "EMPRUNTE" && !chaine.emprunte) {
            chaine.emprunte = true;
            std::cout << chaine.suivant << '\n';
        } else if (commande == "REND" && chaine.emprunte) {
            long long rendue = chaine.suivant;
            chaine.emprunte = false;
            chaine.suivant = (chaine.suivant + 1) % images;
            ++chaine.rendus;
            std::cout << rendue << '\n';
        } else {
            std::cout << "REFUSE" << '\n';
        }

        difference = gauche.rendus - droite.rendus;
        if (difference < 0) {
            difference = -difference;
        }
        if (difference > 1 && desync == 0) {
            desync = tour;
        }
    }
    std::cout << "DECALAGE " << difference << '\n';
    std::cout << "DESYNC " << desync << '\n';
    return 0;
}
