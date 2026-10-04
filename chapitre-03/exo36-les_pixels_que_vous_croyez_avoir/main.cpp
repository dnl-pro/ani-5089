#include <iostream>

int main() {
    int n;
    std::cin >> n;

    int lisible = 0;

    for (int i = 0; i < n; ++i) {
        int largeur;
        int hauteur;
        int echelle;
        int champ;

        std::cin >> largeur >> hauteur >> echelle >> champ;

        int largeur_reelle = largeur * echelle / 100;
        int hauteur_reelle = hauteur * echelle / 100;

        int pixels_par_degre =
            (largeur_reelle + champ / 2) / champ;

        std::cout << largeur_reelle << ' '
                  << hauteur_reelle << ' '
                  << pixels_par_degre << '\n';

        if (pixels_par_degre >= 15) {
            ++lisible;
        }
    }

    std::cout << "LISIBLE " << lisible << '\n';

    return 0;
}
