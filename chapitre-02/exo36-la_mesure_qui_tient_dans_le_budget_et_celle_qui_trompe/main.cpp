#include <iostream>
#include <string>

int main() {
    long long budget;
    std::cin >> budget;

    int S;
    std::cin >> S;

    int trompe = 0;

    for (int i = 0; i < S; ++i) {
        std::string nom;
        long long debug;
        long long release;

        std::cin >> nom >> debug >> release;

        long long facteur = (debug + release / 2) / release;

        bool tient = release <= budget;

        if (debug > budget && tient) {
            ++trompe;
        }

        std::cout << nom << ' ' << facteur << ' '
                  << (tient ? "TIENT" : "DEPASSE") << '\n';
    }

    std::cout << "TROMPE " << trompe << '\n';

    return 0;
}
