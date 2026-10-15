#include <chrono>
#include <iostream>

#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkDeviceInitInfo.h"
#include "NKRHI/Core/NkGraphicsApi.h"

// Exercice 26 : comparer deux interfaces graphiques.
// Interfaces à essayer : SOFTWARE et OPENGL.
//
// Pour chaque essai :
// 1. Initialiser la même salle et la même caméra.
// 2. Créer le périphérique avec CreateForApi.
// 3. Vérifier que le périphérique existe et est valide.
// 4. Configurer le renderer avec la même API.
// 5. Initialiser le renderer et afficher NkGraphicsApiName(api).
// 6. Laisser les premières images de chauffe s'exécuter.
// 7. Mesurer 400 images et calculer le temps moyen en millisecondes.
//
// La création de la fenêtre, du renderer et de la salle doit reprendre
// les déclarations exactes du projet R3D utilisé pour les exercices précédents.

using namespace nkentseu;

void EssayerInterface(NkGraphicsApi api, const NkDeviceInitInfo& init)
{
    std::cout << "\nInterface demandee : "
              << NkGraphicsApiName(api) << '\n';

    // Ne pas utiliser CreateAutoDetect : l'API est imposée ici.
    NkIDevice* device = NkDeviceFactory::CreateForApi(api, init);

    if (device == nullptr) {
        std::cerr << "Refus : impossible de creer le peripherique.\n";
        return;
    }

    // Vérifier ici la validité du périphérique avec l'API R3D.
    // Configurer ensuite le renderer avec exactement la même API :
    // rendererConfig.api = api;
    //
    // Après initialisation du renderer et rendu de la salle :
    // - ignorer les images de chauffe ;
    // - mesurer 400 images avec std::chrono ;
    // - afficher la moyenne en millisecondes par image.
    //
    // La libération du périphérique doit respecter le cycle de vie
    // prévu par R3D.
}

int main()
{
    NkDeviceInitInfo init{};

    // Compléter avec la fenêtre, la surface et les paramètres
    // d'initialisation requis par le programme de la salle.

    EssayerInterface(NkGraphicsApi::NK_GFX_API_SOFTWARE, init);
    EssayerInterface(NkGraphicsApi::NK_GFX_API_OPENGL, init);

    return 0;
}
