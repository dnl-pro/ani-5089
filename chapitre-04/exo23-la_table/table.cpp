#include <NKRenderer/Core/NkRendererTypes.h>
#include <NKRenderer/Mesh/NkMeshSystem.h>
#include <NKRenderer/Tools/Render3D/NkRender3D.h>
#include <NKMath/NkMat.h>

// Exercice : une table composee d'un plateau et de quatre pieds.
// Dimensions en metres, origine au centre du sol.

using namespace nkentseu;
using namespace nkentseu::renderer;

namespace
{
    constexpr float TABLE_LENGTH = 1.20f;
    constexpr float TABLE_DEPTH = 0.80f;
    constexpr float TOP_HEIGHT = 0.75f;
    constexpr float TOP_THICKNESS = 0.05f;
    constexpr float FOOT_SECTION = 0.06f;

    constexpr float TABLE_CENTER_X = 0.60f;
    constexpr float TABLE_CENTER_Z = 0.70f;

    constexpr float TOP_CENTER_Y =
        TOP_HEIGHT - TOP_THICKNESS / 2.0f;

    constexpr float FOOT_HEIGHT =
        TOP_HEIGHT - TOP_THICKNESS;

    constexpr float FOOT_CENTER_Y = FOOT_HEIGHT / 2.0f;

    math::NkMat4f TableTransform(
        const math::NkVec3f& position,
        const math::NkVec3f& size)
    {
        return math::NkMat4f::Translate(position)
             * math::NkMat4f::Scale(size);
    }

    void SubmitPart(
        NkRender3D* render3D,
        NkMeshHandle cube,
        const math::NkVec3f& position,
        const math::NkVec3f& size,
        const math::NkVec3f& color)
    {
        NkDrawCall3D draw{};
        draw.mesh = cube;
        draw.transform = TableTransform(position, size);
        draw.tint = color;
        draw.metallic = 0.0f;
        draw.roughness = 0.75f;

        render3D->Submit(draw);
    }
}

// A appeler apres BeginScene(), avant Flush(commandBuffer).
void SubmitTable(
    NkMeshSystem* meshSystem,
    NkRender3D* render3D)
{
    if (meshSystem == nullptr || render3D == nullptr)
        return;

    const NkMeshHandle cube = meshSystem->GetCube();

    // Plateau : dessus a 0,75 m du sol.
    SubmitPart(
        render3D, cube,
        {TABLE_CENTER_X, TOP_CENTER_Y, TABLE_CENTER_Z},
        {TABLE_LENGTH, TOP_THICKNESS, TABLE_DEPTH},
        {0.48f, 0.25f, 0.12f});

    // Positions des pieds, a l'interieur des quatre coins.
    const float xLeft =
        TABLE_CENTER_X - TABLE_LENGTH / 2.0f + FOOT_SECTION / 2.0f;
    const float xRight =
        TABLE_CENTER_X + TABLE_LENGTH / 2.0f - FOOT_SECTION / 2.0f;
    const float zFront =
        TABLE_CENTER_Z - TABLE_DEPTH / 2.0f + FOOT_SECTION / 2.0f;
    const float zBack =
        TABLE_CENTER_Z + TABLE_DEPTH / 2.0f - FOOT_SECTION / 2.0f;

    const math::NkVec3f footSize = {
        FOOT_SECTION, FOOT_HEIGHT, FOOT_SECTION
    };
    const math::NkVec3f footColor = {0.32f, 0.16f, 0.08f};

    SubmitPart(
        render3D, cube,
        {xLeft, FOOT_CENTER_Y, zFront},
        footSize, footColor);

    SubmitPart(
        render3D, cube,
        {xLeft, FOOT_CENTER_Y, zBack},
        footSize, footColor);

    SubmitPart(
        render3D, cube,
        {xRight, FOOT_CENTER_Y, zFront},
        footSize, footColor);

    SubmitPart(
        render3D, cube,
        {xRight, FOOT_CENTER_Y, zBack},
        footSize, footColor);
}
