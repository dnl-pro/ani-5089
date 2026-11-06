#include <iostream>
#include <string>

int main() {
    long long vol = 0;
    long long n = 0;
    if (!(std::cin >> vol >> n)) {
        return 0;
    }

    long long enVol = 0;
    long long bloque = 0;

    for (long long tour = 1; tour <= n; ++tour) {
        std::string commande;
        if (!(std::cin >> commande)) {
            break;
        }
        bool attend = false;
        if (commande == "SOUMET") {
            if (enVol >= vol) {
                attend = true;
                if (bloque == 0) {
                    bloque = tour;
                }
            } else {
                ++enVol;
            }
        } else if (commande == "FINI") {
            if (enVol > 0) {
                --enVol;
            }
        }
        std::cout << tour << ' ' << commande << ' ' << enVol << ' '
                  << (attend ? "ATTEND" : "PASSE") << '\n';
    }
    std::cout << "BLOQUE " << bloque << '\n';
    return 0;
}
