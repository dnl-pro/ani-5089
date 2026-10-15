#include <NKRenderer/NkRenderer.h>
#include <NKRenderer/Mesh/NkMeshSystem.h>
#include <NKRenderer/Tools/Render3D/NkRender3D.h>
#include <NKRenderer/Core/NkRendererTypes.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

// Exercice 27 — Cent cubes et le premier mur du réel.
// À intégrer à la boucle de scène NKRenderer déjà initialisée dans les exercices
// précédents : renderer, caméra, lumière et matériau du cube restent ceux de la salle.

namespace Exo27
{
    using namespace nkentseu::renderer;

    struct CubePosition
    {
        float x;
        float y;
        float z;
    };

    static std::vector<CubePosition> CreerGrille(unsigned int nombre,
                                                  float espacement = 2.0f)
    {
        std::vector<CubePosition> positions;
        positions.reserve(nombre);
        const unsigned int cote = std::max(
            1u, static_cast<unsigned int>(std::ceil(std::sqrt(static_cast<double>(nombre)))));

        for (unsigned int i = 0; i < nombre; ++i)
        {
            const unsigned int colonne = i % cote;
            const unsigned int ligne = i / cote;
            positions.push_back({
                (static_cast<float>(colonne) - static_cast<float>(cote - 1) / 2.0f) * espacement,
                0.0f,
                (static_cast<float>(ligne) - static_cast<float>(cote - 1) / 2.0f) * espacement
            });
        }
        return positions;
    }

    template <typename PompeEvenements, typename RenduImage>
    static double MesurerFPS(PompeEvenements&& pomperEvenements,
                             RenduImage&& rendreImage,
                             unsigned int nombreImages = 400)
    {
        nombreImages = std::max(nombreImages, 400u);
        const auto debut = std::chrono::steady_clock::now();
        for (unsigned int i = 0; i < nombreImages; ++i)
        {
            pomperEvenements();
            rendreImage();
        }
        const auto fin = std::chrono::steady_clock::now();
        const std::chrono::duration<double> duree = fin - debut;
        return duree.count() > 0.0 ? nombreImages / duree.count() : 0.0;
    }

    template <typename Renderer>
    static std::uint64_t LireDrawCalls(const Renderer* renderer)
    {
        return static_cast<std::uint64_t>(renderer->GetStats().drawCalls);
    }

    // STRATÉGIE 1 — un cube par soumission.
    // Pour chaque position, construire le NkDrawCall3D comme dans la salle
    // existante (même mesh GetCube(), même matériau et transformation à la position),
    // puis appeler render3D->Submit(drawCall). Ne pas recréer le mesh à chaque image.

    // STRATÉGIE 2 — soumission groupée/instanciée.
    // Le kit expose NkDrawCallInstanced et NkRender3D::SubmitInstanced ; construire
    // l'appel avec le même mesh cube, le même matériau et les transformations de la
    // grille, puis le soumettre une fois. Respecter la signature et les champs exacts
    // de NkDrawCallInstanced déclarés dans Core/NkRendererTypes.h.

    // À appeler depuis la boucle de rendu déjà en place :
    // const double fps = MesurerFPS(pomperEvenements, rendreImage, 400);
    // std::cout << "FPS moyen (400 images) : " << fps << '\n';
    // std::cout << "drawCalls : " << LireDrawCalls(renderer) << '\n';
    // Lire le compteur après la soumission/rendu de l'image mesurée, selon le cycle
    // BeginFrame() / EndFrame() / Present() de l'application existante.
}
