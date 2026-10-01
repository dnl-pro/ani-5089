#include <iostream>
#include <map>
#include <sstream>
#include <string>

int main() {
    int V;
    std::cin >> V;

    std::map<std::string, std::string> machine;

    for (int i = 0; i < V; ++i) {
        std::string ligne;
        std::cin >> ligne;

        std::size_t position = ligne.find('=');

        std::string cle = ligne.substr(0, position);
        std::string valeur = ligne.substr(position + 1);

        machine[cle] = valeur;
    }

    int F;
    std::cin >> F;
    std::cin.ignore();

    for (int i = 0; i < F; ++i) {
        std::string condition;
        std::getline(std::cin, condition);

        std::istringstream iss(condition);
        std::string terme;
        bool filtre_valide = true;

        while (iss >> terme) {
            if (terme == "&&") {
                continue;
            }

            bool inverse = false;

            if (!terme.empty() && terme[0] == '!') {
                inverse = true;
                terme = terme.substr(1);
            }

            std::size_t position = terme.find('=');

            std::string cle = terme.substr(0, position);
            std::string valeur = terme.substr(position + 1);

            bool terme_vrai = false;

            auto it = machine.find(cle);

            if (it != machine.end()) {
                terme_vrai = (it->second == valeur);
            }

            if (inverse) {
                terme_vrai = !terme_vrai;
            }

            if (!terme_vrai) {
                filtre_valide = false;
            }
        }

        if (filtre_valide) {
            std::cout << "OUI\n";
        } else {
            std::cout << "NON\n";
        }
    }

    return 0;
}
