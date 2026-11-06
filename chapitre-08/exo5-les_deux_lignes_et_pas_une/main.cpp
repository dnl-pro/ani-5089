#include <iostream>
#include <string>

int main() {
    long long largeur = 0;
    long long hauteur = 0;
    long long n = 0;
    if (!(std::cin >> largeur >> hauteur >> n)) {
        return 0;
    }

    long long bloquantes = 0;

    for (long long i = 0; i < n; ++i) {
        std::string nom;
        int cible = 0;
        int taille = 0;
        long long lc = 0;
        long long hc = 0;
        if (!(std::cin >> nom >> cible >> taille >> lc >> hc)) {
            break;
        }

        long long vraieLargeur = largeur;
        long long vraieHauteur = hauteur;
        std::string verdict;

        if (cible == 0) {
            verdict = "PAS RELIEE";
        } else if (taille == 1) {
            vraieLargeur = lc;
            vraieHauteur = hc;
            verdict = "OK";
        } else if (lc != largeur || hc != hauteur) {
            verdict = "BLOQUE";
            ++bloquantes;
        } else {
            verdict = "SANS EFFET";
        }

        std::cout << nom << ' ' << vraieLargeur << ' ' << vraieHauteur << ' '
                  << verdict << '\n';
    }
    std::cout << "BLOQUANTES " << bloquantes << '\n';
    return 0;
}
