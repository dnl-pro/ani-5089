#include <NKRenderer/Core/NkSceneContext.h>
#include <NKRenderer/Core/NkCamera.h>
#include <NKRenderer/Core/NkRendererTypes.h>
#include <NKRenderer/Mesh/NkMeshSystem.h>
#include <NKRenderer/Tools/Render3D/NkRender3D.h>

#include <iostream>

// Exercice : un cube et rien d'autre.
// Le renderer, le device et le command buffer doivent être créés
// et fournis par l'application hôte conformément à son backend R3D.

using namespace nkentseu;
using namespace nkentseu::renderer;

// Construit les données de scène nécessaires au cube.
static NkSceneContext CreateCubeScene(float32 aspect)
{
    NkSceneContext scene{};

    // Caméra à hauteur d'œil, orientée vers l'origine.
    NkCamera3DData cameraData{};
    cameraData.position = {0.0f, 1.6f, 5.0f};
    cameraData.target   = {0.0f, 0.0f, 0.0f};
    cameraData.up       = {0.0f, 1.0f, 0.0f};
    cameraData.fovY     = 65.0f;
    cameraData.aspect   = aspect;
    cameraData.nearPlane = 0.1f;
    cameraData.farPlane  = 100.0f;

    scene.camera.SetData(cameraData);

    // Lumière directionnelle pour éclairer le cube.
    NkLightDesc light{};
    light.type      = NkLightType::NK_DIRECTIONAL;
    light.direction = {-0.4f, -1.0f, -0.3f};
    light.color     = {1.0f, 0.95f, 0.85f};
    light.intensity = 2.0f;

    scene.lights.push_back(light);

    return scene;
}

// Soumet le cube au système de rendu 3D.
// meshSystem et render3D sont fournis par le renderer initialisé.
static bool SubmitCube(
    NkMeshSystem* meshSystem,
    NkRender3D* render3D)
{
    if (meshSystem == nullptr || render3D == nullptr)
    {
        std::cerr << "[ERREUR] Renderer ou système de maillage nul.\n";
        return false;
    }

    // Utiliser la primitive cube intégrée au système de maillage.
    const NkMeshHandle cube = meshSystem->GetCube();

    NkDrawCall3D draw{};
    draw.mesh = cube;
    draw.transform = NkMat4f::Identity();
    draw.tint = {0.2f, 0.55f, 0.95f};
    draw.metallic = 0.0f;
    draw.roughness = 0.45f;

    render3D->Submit(draw);
    return true;
}

// La boucle d'application doit, à chaque frame :
// 1. construire le NkSceneContext avec CreateCubeScene(aspect) ;
// 2. appeler render3D->BeginScene(scene) ;
// 3. appeler SubmitCube(meshSystem, render3D) ;
// 4. enregistrer le rendu avec Flush(commandBuffer) ;
// 5. terminer et présenter la frame via le renderer hôte.
//
// Le cycle de vie du device, du renderer et du command buffer dépend
// du backend choisi par l'application ; il n'est pas simulé ici.
