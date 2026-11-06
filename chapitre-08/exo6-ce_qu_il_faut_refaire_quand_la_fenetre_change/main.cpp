#include <iostream>
#include <sstream>
#include <string>

int main() {
    long long n = 0;
    if (!(std::cin >> n)) {
        return 0;
    }
    std::string ligne;
    std::getline(std::cin, ligne);

    long long survivants = 0;
    long long lus = 0;

    while (lus < n && std::getline(std::cin, ligne)) {
        std::istringstream mots(ligne);
        std::string nom;
        if (!(mots >> nom)) {
            continue;
        }
        ++lus;

        long long rang = 0;
        long long rangDetruit = -1;
        long long rangRecree = -1;
        bool reenregistre = false;

        std::string mot;
        while (mots >> mot) {
            ++rang;
            if (mot == "DETRUIT" && rangDetruit < 0) {
                rangDetruit = rang;
            } else if (mot == "RECREE" && rangRecree < 0) {
                rangRecree = rang;
            } else if (mot == "REENREGISTRE") {
                reenregistre = true;
            }
        }

        std::string verdict;
        if (rangDetruit >= 0 && rangRecree >= 0 && rangRecree < rangDetruit) {
            verdict = "MAUVAIS ORDRE";
        } else if (rangDetruit < 0 || rangRecree < 0) {
            verdict = "CHAINE PERIMEE";
        } else if (!reenregistre) {
            verdict = "POIGNEES PERDUES";
        } else {
            verdict = "SURVIT";
            ++survivants;
        }
        std::cout << nom << ' ' << verdict << '\n';
    }
    std::cout << "SURVIVANTS " << survivants << '\n';
    return 0;
}
