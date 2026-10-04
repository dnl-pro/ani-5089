#include <iostream>
#include <string>

int main() {
    int p;
    int f;
    std::cin >> p >> f;

    int sans_releve = 0;
    int premier = 0;

    for (int tour = 1; tour <= f; ++tour) {
        std::string action;
        std::cin >> action;

        if (action == "releve") {
            sans_releve = 0;
        } else if (action == "travaille") {
            ++sans_releve;
        }

        bool morte = sans_releve >= p;

        if (morte && premier == 0) {
            premier = tour;
        }

        std::cout << sans_releve << ' '
                  << (morte ? "MORTE" : "VIVANTE") << '\n';
    }

    std::cout << "PREMIER " << premier << '\n';

    return 0;
}
