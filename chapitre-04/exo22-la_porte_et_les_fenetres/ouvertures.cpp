#include <NKRenderer/Core/NkRendererTypes.h>
#include <NKRenderer/Mesh/NkMeshSystem.h>
#include <NKRenderer/Tools/Render3D/NkRender3D.h>
#include <NKMath/NkMat.h>

// Exercice : la porte et les deux fenêtres de la salle.
// Salle : 4 m x 4 m, hauteur 2,50 m.
// Murs : épaisseur 0,10 m.
// Les cubes sont supposés mesurer 1 unité avant mise à l'échelle.

using namespace nkentseu;
using namespace nkentseu::renderer;

namespace
{
    constexpr float WALL_THICKNESS = 0.10f;
    constexpr float PANEL_THICKNESS = 0.02f;
    constexpr float ROOM_HALF_SIZE = 2.0f;

    // Face intérieure du mur du fond : z = -1,95 m.
    constexpr float BACK_WALL_INNER_Z =
        -ROOM_HALF_SIZE + WALL_THICKNESS / 2.0f;

    // Face intérieure du mur de gauche : x = -1,95 m.
    constexpr float LEFT_WALL_INNER_X =
        -ROOM_HALF_SIZE + WALL_THICKNESS / 2.0f;

    // Décalage vers l'intérieur pour supprimer le scintillement.
    constexpr float FLICKER_OFFSET = 0.03f;

    math::NkMat4f PanelTransform(
        const math::NkVec3f& position,
        const math::NkVec3f& size)
    {
        return math::NkMat4f::Translate(position)
             * math::NkMat4f::Scale(size);
    }

    void SubmitPanel(
        NkRender3D* render3D,
        NkMeshHandle cube,
        const math::NkVec3f& position,
        const math::NkVec3f& size,
        const math::NkVec3f& color)
    {
        NkDrawCall3D draw{};
        draw.mesh = cube;
        draw.transform = PanelTransform(position, size);
        draw.tint = color;
        draw.metallic = 0.0f;
        draw.roughness = 0.8f;

        render3D->Submit(draw);
    }
}

// Corrigé = false : positions initiales, faces avant exactement
// dans le plan intérieur des murs, pour observer le défaut.
// Corrigé = true : panneaux avancés de 3 cm vers l'intérieur.
void SubmitOpenings(
    NkMeshSystem* meshSystem,
    NkRender3D* render3D,
    bool corrigeClignotement = true)
{
    if (meshSystem == nullptr || render3D == nullptr)
        return;

    const NkMeshHandle cube = meshSystem->GetCube();
    const float offset = corrigeClignotement ? FLICKER_OFFSET : 0.0f;

    // PORTE : largeur 0,90 m, hauteur 2,00 m,
    // posée au sol, centre x = -0,70 m.
    // Son épaisseur est suivant z.
    const float doorCenterZ =
        BACK_WALL_INNER_Z - PANEL_THICKNESS / 2.0f + offset;

    SubmitPanel(
        render3D, cube,
        {-0.70f, 1.00f, doorCenterZ},
        {0.90f, 2.00f, PANEL_THICKNESS},
        {0.35f, 0.16f, 0.07f});

    // FENÊTRE 1 : largeur suivant z = 1,20 m,
    // hauteur = 1,00 m, centre à y = 1,40 m.
    const float windowCenterX =
        LEFT_WALL_INNER_X - PANEL_THICKNESS / 2.0f + offset;

    SubmitPanel(
        render3D, cube,
        {windowCenterX, 1.40f, -0.90f},
        {PANEL_THICKNESS, 1.00f, 1.20f},
        {0.20f, 0.65f, 0.90f});

    // FENÊTRE 2 : mêmes dimensions, centre z = +0,90 m.
    SubmitPanel(
        render3D, cube,
        {windowCenterX, 1.40f, 0.90f},
        {PANEL_THICKNESS, 1.00f, 1.20f},
        {0.20f, 0.65f, 0.90f});
}

// Dans la boucle de rendu de la salle :
// render3D->BeginScene(scene);
// SubmitOpenings(meshSystem, render3D, false); // observer le défaut
// SubmitOpenings(meshSystem, render3D, true);  // rendu corrigé
// render3D->Flush(commandBuffer);

