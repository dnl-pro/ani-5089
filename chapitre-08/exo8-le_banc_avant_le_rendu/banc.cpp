#include <cstdint>
#include <iostream>

#include "chaine.h"

namespace {

int gEchecs = 0;

void Verifier(bool condition, const char* texte) {
    std::cout << texte << " : " << (condition ? "refuse" : "ACCEPTE A TORT") << '\n';
    if (!condition) {
        ++gEchecs;
    }
}

}  // namespace

int main() {
    ChaineEchange chaine;
    if (!chaine.Creer(3)) {
        std::cout << "creation impossible" << '\n';
        return 1;
    }
    std::cout << "anneau de " << chaine.Nombre() << " images" << '\n';

    // Six allures : emprunter, puis rendre. L'anneau fait deux tours.
    for (int allure = 1; allure <= 6; ++allure) {
        std::uint32_t pris = 0;
        std::uint32_t rendu = 0;
        bool a = chaine.Emprunter(pris);
        bool b = chaine.Rendre(rendu);
        std::cout << "allure " << allure << " : emprunte " << pris
                  << ", rend " << rendu << '\n';
        if (!a || !b || pris != rendu || pris != static_cast<std::uint32_t>((allure - 1) % 3)) {
            ++gEchecs;
        }
    }

    // Les trois refus.
    std::uint32_t index = 0;
    chaine.Emprunter(index);
    Verifier(!chaine.Emprunter(index), "double emprunt");
    chaine.Rendre(index);
    Verifier(!chaine.Rendre(index), "rendre sans emprunt");

    ChaineEchange trop;
    Verifier(!trop.Creer(ChaineEchange::MaxImages + 1), "chaine plus grande que le maximum");

    std::cout << (gEchecs == 0 ? "banc : tout est bon" : "banc : echec") << '\n';
    return gEchecs == 0 ? 0 : 1;
}
