#include <iostream>
#include <map>
#include <string>

int main() {
    long long images = 0;
    long long n = 0;
    if (!(std::cin >> images >> n)) {
        return 0;
    }

    // La poignee est gardee telle quelle, sans etre interpretee.
    std::map<long long, std::string> poignees;

    for (long long i = 0; i < n; ++i) {
        std::string commande;
        if (!(std::cin >> commande)) {
            break;
        }
        if (commande == "POSE") {
            long long index = 0;
            std::string valeur;
            if (!(std::cin >> index >> valeur)) {
                break;
            }
            if (index < 0 || index >= images) {
                std::cout << "HORS ANNEAU" << '\n';
            } else {
                poignees[index] = valeur;
            }
        } else if (commande == "LIT") {
            long long index = 0;
            if (!(std::cin >> index)) {
                break;
            }
            std::map<long long, std::string>::const_iterator it = poignees.find(index);
            if (index < 0 || index >= images || it == poignees.end()) {
                std::cout << "VIDE" << '\n';
            } else {
                std::cout << it->second << '\n';
            }
        }
    }
    std::cout << "POSEES " << poignees.size() << '\n';
    return 0;
}
