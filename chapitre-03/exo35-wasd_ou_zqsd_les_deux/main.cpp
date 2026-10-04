#include <iostream>
#include <string>
#include <set>

int main() {
    int n;
    std::cin >> n;

    for (int i = 0; i < n; ++i) {
        std::set<std::string> touches;
        std::string touche;

        std::cin >> touche;

        if (touche != "rien") {
            touches.insert(touche);

            while (std::cin.peek() != '\n' && std::cin.peek() != EOF) {
                std::cin >> touche;
                touches.insert(touche);
            }
        }

        int avance = 0;
        int cote = 0;

        if (touches.count("W") || touches.count("Z")) {
            ++avance;
        }

        if (touches.count("S")) {
            --avance;
        }

        if (touches.count("A") || touches.count("Q")) {
            --cote;
        }

        if (touches.count("D")) {
            ++cote;
        }

        std::cout << avance << ' ' << cote << '\n';
    }

    return 0;
}
