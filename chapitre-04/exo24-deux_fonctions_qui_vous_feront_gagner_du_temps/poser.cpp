#include <NKRenderer/Core/NkRendererTypes.h>
#include <NKRenderer/Mesh/NkMeshSystem.h>
#include <NKRenderer/Tools/Render3D/NkRender3D.h>
#include <NKMath/NkMat.h>

// Exercice 24 : factoriser les transformations de placement.
// Les dimensions et les positions sont exprimees en metres.

using namespace nkentseu;
using namespace nkentseu::renderer;

// Place un objet sur le sol : le bas de l'objet est a y = 0.
math::NkMat4f PoserAuSol(math::NkVec3f taille, float x, float z)
{
    const math::NkVec3f position = {
        x,
        taille.y / 2.0f,
        z
    };

    return math::NkMat4f::Translate(position)
         * math::NkMat4f::Scale(taille);
}

// Place un objet sur une table dont la surface est a y = 0,75 m.
math::NkMat4f PoserSurTable(math::NkVec3f taille, float x, float z)
{
    const math::NkVec3f position = {
        x,
        0.75f + taille.y / 2.0f,
        z
    };

    return math::NkMat4f::Translate(position)
         * math::NkMat4f::Scale(taille);
}

namespace
{
    void SubmitPart(
        NkRender3D* render3D,
        NkMeshHandle cube,
        const math::NkMat4f& transform,
        const math::NkVec3f& color)
    {
        NkDrawCall3D draw{};
        draw.mesh = cube;
        draw.transform = transform;
        draw.tint = color;
        draw.metallic = 0.0f;
        draw.roughness = 0.75f;

        render3D->Submit(draw);
    }
}

// Exemple d'utilisation des deux fonctions avec la table de l'exercice 23.
// A appeler apres BeginScene() et avant Flush(commandBuffer).
void SubmitTableAvecPoser(
    NkMeshSystem* meshSystem,
    NkRender3D* render3D)
{
    if (meshSystem == nullptr || render3D == nullptr)
        return;

    const NkMeshHandle cube = meshSystem->GetCube();

    constexpr float tableLength = 1.20f;
    constexpr float tableDepth = 0.80f;
    constexpr float topHeight = 0.75f;
    constexpr float topThickness = 0.05f;
    constexpr float footSection = 0.06f;
    constexpr float centerX = 0.60f;
    constexpr float centerZ = 0.70f;
    constexpr float footHeight = topHeight - topThickness;

    // Plateau : son dessus atteint exactement 0,75 m.
    SubmitPart(
        render3D, cube,
        math::NkMat4f::Translate(
            {centerX, topHeight - topThickness / 2.0f, centerZ}
        ) * math::NkMat4f::Scale(
            {tableLength, topThickness, tableDepth}
        ),
        {0.48f, 0.25f, 0.12f}
    );

    const float xLeft =
        centerX - tableLength / 2.0f + footSection / 2.0f;
    const float xRight =
        centerX + tableLength / 2.0f - footSection / 2.0f;
    const float zFront =
        centerZ - tableDepth / 2.0f + footSection / 2.0f;
    const float zBack =
        centerZ + tableDepth / 2.0f - footSection / 2.0f;

    const math::NkVec3f footSize = {
        footSection, footHeight, footSection
    };
    const math::NkVec3f footColor = {0.32f, 0.16f, 0.08f};

    SubmitPart(render3D, cube,
        PoserAuSol(footSize, xLeft, zFront), footColor);
    SubmitPart(render3D, cube,
        PoserAuSol(footSize, xLeft, zBack), footColor);
    SubmitPart(render3D, cube,
        PoserAuSol(footSize, xRight, zFront), footColor);
    SubmitPart(render3D, cube,
        PoserAuSol(footSize, xRight, zBack), footColor);

    // Petit objet pose sur le plateau pour illustrer la seconde fonction.
    const math::NkVec3f objectSize = {0.15f, 0.20f, 0.15f};
    SubmitPart(
        render3D, cube,
        PoserSurTable(objectSize, centerX, centerZ),
        {0.20f, 0.65f, 0.90f}
    );
}

