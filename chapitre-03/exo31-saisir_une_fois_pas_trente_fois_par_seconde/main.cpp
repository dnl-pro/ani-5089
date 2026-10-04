#include <iostream>
#include <string>

int main() {
    int n;
    std::cin >> n;

    int sans_garde = 0;

    for (int i = 0; i < n; ++i) {
        std::string evenement;
        std::cin >> evenement;

        if (evenement == "enfonce") {
            std::cout << "SAISIR\n";
            ++sans_garde;
        } else if (evenement == "repete") {
            std::cout << "RIEN\n";
            ++sans_garde;
        } else if (evenement == "relache") {
            std::cout << "LACHER\n";
        }
    }

    std::cout << "SANS_GARDE " << sans_garde << '\n';

    return 0;
}
