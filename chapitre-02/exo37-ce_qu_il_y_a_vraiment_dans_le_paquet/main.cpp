#include <iostream>
#include <string>

bool se_termine_par(const std::string& texte, const std::string& suffixe) {
    if (texte.size() < suffixe.size()) {
        return false;
    }

    return texte.compare(
        texte.size() - suffixe.size(),
        suffixe.size(),
        suffixe
    ) == 0;
}

int main() {
    std::string architecture;
    std::cin >> architecture;

    int F;
    std::cin >> F;

    long long taille_totale = 0;
    bool signe = false;
    bool abi = false;
    int inutile = 0;

    const std::string prefixe_lib = "lib/";
    const std::string prefixe_abi = "lib/" + architecture + "/";

    for (int i = 0; i < F; ++i) {
        std::string chemin;
        long long taille;

        std::cin >> chemin >> taille;

        taille_totale += taille;

        if (chemin.rfind("META-INF/", 0) == 0 &&
            (se_termine_par(chemin, ".RSA") ||
             se_termine_par(chemin, ".DSA") ||
             se_termine_par(chemin, ".EC"))) {
            signe = true;
        }

        if (chemin.rfind(prefixe_abi, 0) == 0) {
            abi = true;
        } else if (chemin.rfind(prefixe_lib, 0) == 0) {
            ++inutile;
        }
    }

    std::cout << taille_totale << '\n';
    std::cout << (signe ? "SIGNE" : "NON SIGNE") << '\n';
    std::cout << (abi ? "ABI OUI" : "ABI NON") << '\n';
    std::cout << "INUTILE " << inutile << '\n';

    return 0;
}
