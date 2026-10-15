#include <iostream>

int main() {
    std::cout << "Programme lancé." << std::endl;

    int* pointeur = nullptr;

    // Plantage volontaire : dereferencement d'un pointeur nul.
    std::cout << *pointeur << std::endl;

    return 0;
}
