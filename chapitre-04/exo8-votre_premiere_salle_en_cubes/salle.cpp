#include <NKRenderer/NkRenderer.h>
#include <NKRenderer/Mesh/NkMeshSystem.h>
#include <NKRenderer/Tools/Render3D/NkRender3D.h>
#include <NKRenderer/Core/NkRendererTypes.h>

#include <array>
#include <iostream>

// Exercice 8 — Votre première salle en cubes.
// À appeler depuis la scène NKRenderer déjà initialisée (caméra, lumière,
// matériau et cycle BeginFrame/BeginScene/Flush/EndFrame/Present de l'application).
// La configuration du renderer doit activer explicitement NK_SS_RENDER3D.

namespace Exo8
{
    using namespace nkentseu::renderer;

    struct FaceSalle
    {
        NkMat4f transformation;
    };

    // Convention exigée par l'exercice : translation puis échelle.
    // Le cube de base est supposé centré à l'origine et de côté 1 mètre.
    static NkMat4f Transformation(float x, float y, float z,
                                  float sx, float sy, float sz)
    {
        return NkMat4f::Translation(x, y, z) * NkMat4f::Scale(sx, sy, sz);
    }

    // La salle mesure 4 m x 4 m x 2,5 m. L'origine est au centre du sol.
    // Les murs ont 0,10 m d'épaisseur et leur centre est à y = 1,25 m :
    // leur face inférieure repose donc sur le sol y = 0.
    static void DessinerSalle(NkRenderer* renderer, NkMaterial* materiau)
    {
        if (renderer == nullptr || materiau == nullptr)
            return;

        NkMeshSystem* meshes = renderer->GetMeshSystem();
        NkRender3D* rendu3D = renderer->GetRender3D();
        if (meshes == nullptr || rendu3D == nullptr)
            return;

        auto* cube = meshes->GetCube();
        auto* plan = meshes->GetPlane();
        if (cube == nullptr || plan == nullptr)
            return;

        // Sol : plan horizontal de 4 m x 4 m, posé à y = 0.
        NkDrawCall3D sol{};
        sol.mesh = plan;
        sol.material = materiau;
        sol.transform = Transformation(0.0f, 0.0f, 0.0f, 4.0f, 1.0f, 4.0f);
        rendu3D->Submit(sol);

        // Deux murs latéraux : largeur 4 m, hauteur 2,5 m, épaisseur 0,10 m.
        const std::array<FaceSalle, 4> murs = {{
            {Transformation( 0.0f, 1.25f, -1.95f, 4.0f, 2.5f, 0.10f)},
            {Transformation( 0.0f, 1.25f,  1.95f, 4.0f, 2.5f, 0.10f)},
            {Transformation(-1.95f, 1.25f, 0.0f, 0.10f, 2.5f, 4.0f)},
            {Transformation( 1.95f, 1.25f, 0.0f, 0.10f, 2.5f, 4.0f)}
        }};

        for (const FaceSalle& mur : murs)
        {
            NkDrawCall3D appel{};
            appel.mesh = cube;
            appel.material = materiau;
            appel.transform = mur.transformation;
            rendu3D->Submit(appel);
        }

        // Plafond : face supérieure à y = 2,5 m.
        NkDrawCall3D plafond{};
        plafond.mesh = cube;
        plafond.material = materiau;
        plafond.transform = Transformation(0.0f, 2.50f, 0.0f, 4.0f, 0.10f, 4.0f);
        rendu3D->Submit(plafond);
    }

    // Dans l'initialisation du renderer, renseigner explicitement :
    // config.subsystems = NK_SS_RENDER3D; // seul sous-système requis ici.
    // Afficher le nom lisible du backend réellement sélectionné par le kit,
    // puis recopier exactement cette ligne dans backend.txt.
}
