#include <iostream>
#include <string>

int main() {
    long long n = 0;
    if (!(std::cin >> n)) {
        return 0;
    }

    bool acquise = false;
    bool attente = false;
    long long fautes = 0;

    for (long long tour = 1; tour <= n; ++tour) {
        std::string appel;
        if (!(std::cin >> appel)) {
            break;
        }
        std::string resultat = "OK";
        if (appel == "ACQUIERT") {
            if (acquise) {
                resultat = "DEJA ACQUISE";
            } else {
                acquise = true;
            }
        } else if (appel == "DESSINE") {
            if (!acquise) {
                resultat = "PAS ACQUISE";
            }
        } else if (appel == "REND") {
            if (!acquise) {
                resultat = "RIEN ACQUIS";
            } else {
                acquise = false;
                attente = true;
            }
        } else if (appel == "SOUMET") {
            if (!attente) {
                resultat = "PAS RENDUE";
            } else {
                attente = false;
            }
        }
        if (resultat != "OK") {
            ++fautes;
        }
        std::cout << tour << ' ' << appel << ' ' << resultat << '\n';
    }
    std::cout << "FAUTES " << fautes << '\n';
    return 0;
}
