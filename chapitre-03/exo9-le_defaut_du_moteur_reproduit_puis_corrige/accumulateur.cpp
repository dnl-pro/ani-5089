#include <iostream>

struct Accumulateur {
    int totalX = 0;
    int totalY = 0;

    void ajouter(int rawDeltaX, int rawDeltaY) {
        totalX += rawDeltaX;
        totalY += rawDeltaY;
    }

    void consommer() {
        std::cout << totalX << ' ' << totalY << '\n';

        totalX = 0;
        totalY = 0;
    }
};

int main() {
    Accumulateur accumulateur;

    accumulateur.ajouter(5, 2);
    accumulateur.ajouter(3, -1);
    accumulateur.consommer();

    accumulateur.ajouter(0, 4);
    accumulateur.consommer();

    return 0;
}
