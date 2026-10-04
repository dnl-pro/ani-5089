#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

struct Rappel {
    int id;
    std::string type;
};

int main() {
    int n;
    std::cin >> n;

    std::vector<Rappel> registre;

    for (int i = 0; i < n; ++i) {
        std::string commande;
        std::cin >> commande;

        if (commande == "poser") {
            Rappel rappel;
            std::cin >> rappel.id >> rappel.type;

            registre.erase(
                std::remove_if(registre.begin(), registre.end(),
                    [&](const Rappel& r) {
                        return r.id == rappel.id;
                    }),
                registre.end()
            );

            registre.push_back(rappel);

        } else if (commande == "retirer") {
            int id;
            std::cin >> id;

            registre.erase(
                std::remove_if(registre.begin(), registre.end(),
                    [id](const Rappel& r) {
                        return r.id == id;
                    }),
                registre.end()
            );

        } else if (commande == "envoyer") {
            std::string type;
            std::cin >> type;

            bool trouve = false;

            for (const Rappel& rappel : registre) {
                if (rappel.type == type) {
                    if (trouve) {
                        std::cout << ' ';
                    }
                    std::cout << rappel.id;
                    trouve = true;
                }
            }

            if (!trouve) {
                std::cout << "AUCUN";
            }

            std::cout << '\n';
        }
    }

    return 0;
}
