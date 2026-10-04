#include <iostream>
#include <string>

int main() {
    int n;
    std::cin >> n;

    int total_dx = 0;
    int total_dy = 0;
    int moteur_x = 0;
    int moteur_y = 0;

    for (int i = 0; i < n; ++i) {
        std::string evenement;
        std::cin >> evenement;

        if (evenement == "bouge") {
            int dx;
            int dy;
            std::cin >> dx >> dy;

            total_dx += dx;
            total_dy += dy;

            moteur_x = dx;
            moteur_y = dy;
        } else if (evenement == "image") {
            std::cout << total_dx << ' '
                      << total_dy << ' '
                      << moteur_x << ' '
                      << moteur_y << '\n';

            total_dx = 0;
            total_dy = 0;
        }
    }

    return 0;
}
