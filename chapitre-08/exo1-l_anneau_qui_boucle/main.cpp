#include <iostream>
#include <string>

int main() {
    long long images = 0;
    long long n = 0;
    if (!(std::cin >> images >> n)) {
        return 0;
    }
    if (images < 1) {
        images = 1;
    }

    long long suivant = 0;
    bool emprunte = false;
    long long prets = 0;

    for (long long tour = 1; tour <= n; ++tour) {
        std::string commande;
        if (!(std::cin >> commande)) {
            break;
        }
        std::cout << tour << ' ' << commande << ' ';
        if (commande == "EMPRUNTE" && !emprunte) {
            emprunte = true;
            ++prets;
            std::cout << suivant << '\n';
        } else if (commande == "REND" && emprunte) {
            long long rendue = suivant;
            emprunte = false;
            suivant = (suivant + 1) % images;
            std::cout << rendue << '\n';
        } else {
            std::cout << "REFUSE" << '\n';
        }
    }
    std::cout << "PRETS " << prets << '\n';
    return 0;
}
