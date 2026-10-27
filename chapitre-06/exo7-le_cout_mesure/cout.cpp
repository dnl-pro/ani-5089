// =============================================================================
// Chapitre 6, exercice 7 — le coût mesuré.
//
// Au démarrage de la salle, avant d'ouvrir la fenêtre, le programme chronomètre un million
// de fois la construction de l'orientation (lacet puis tangage) de deux façons :
//   - par matrices : RotationY * RotationX, puis conversion en quaternion (le simulateur) ;
//   - directement  : RotateY * RotateX sur des quaternions, puis normalisation.
// Chaque version est mesurée trois fois, avec NkChrono ; les quatre composantes du résultat
// s'ajoutent à une somme imprimée, pour que le compilateur ne supprime pas les boucles.
// Les deux versions donnent la même rotation, mais les deux sommes imprimées diffèrent : la
// conversion depuis une matrice renvoie parfois q et parfois -q, qui est la même rotation.
// La salle s'ouvre ensuite : la souris tourne la tête (lacet), Q quitte.
// =============================================================================
#include <cmath>
#include <cstdio>

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkAngle.h"
#include "NKMath/NkMat.h"
#include "NKMath/NkQuat.h"
#include "NKTime/NkChrono.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkEventDispatcher.h"

#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Materials/NkMaterial.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Shadow/NkVirtualShadowMaps.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

namespace {
    constexpr float TANGAGE_DEG = 30.f;      // fixé : seule la souris pilote le lacet
    constexpr float SENSIBILITE = 0.002f;    // radians par compte du capteur
    constexpr float PI_FLOAT = 3.14159265358979323846f;
}

// ── Le coût de l'orientation : matrices puis conversion, contre produit direct ──
namespace {
    constexpr int ITERATIONS = 1000000;   // un million de tours par boucle
    constexpr int MESURES = 3;            // chaque version est mesurée trois fois

    // Les angles changent à chaque tour : le compilateur ne peut pas calculer le résultat à l'avance.
    inline float LacetDuTour(int i) { return (float)(i % 3600) * 0.1f; }
    inline float TangageDuTour(int i) { return (float)(i % 1200) * 0.1f - 60.f; }

    // La version du simulateur : deux matrices, leur produit, puis la conversion en quaternion.
    NkQuatf ParMatrices(float lacetDeg, float tangageDeg) {
        const NkMat4f rotation = NkMat4f::RotationY(NkAngle(lacetDeg)) *
                                 NkMat4f::RotationX(NkAngle(tangageDeg));
        return NkQuatf(rotation).Normalized();
    }

    // La version directe : le produit des deux quaternions, dans le même ordre que les matrices.
    NkQuatf ParQuaternions(float lacetDeg, float tangageDeg) {
        return (NkQuatf::RotateY(NkAngle(lacetDeg)) * NkQuatf::RotateX(NkAngle(tangageDeg))).Normalized();
    }

    // Les quatre composantes s'ajoutent à une somme que l'appelant imprime : sans cette lecture,
    // le compilateur pourrait supprimer une boucle dont personne ne lit le résultat.
    double MesurerMatrices(double &somme) {
        NkChrono chrono;
        double s = 0.0;
        for (int i = 0; i < ITERATIONS; ++i) {
            const NkQuatf q = ParMatrices(LacetDuTour(i), TangageDuTour(i));
            s += (double)q.x + (double)q.y + (double)q.z + (double)q.w;
        }
        const double ms = (double)chrono.Elapsed().ToMilliseconds();
        somme = s;
        return ms;
    }

    double MesurerQuaternions(double &somme) {
        NkChrono chrono;
        double s = 0.0;
        for (int i = 0; i < ITERATIONS; ++i) {
            const NkQuatf q = ParQuaternions(LacetDuTour(i), TangageDuTour(i));
            s += (double)q.x + (double)q.y + (double)q.z + (double)q.w;
        }
        const double ms = (double)chrono.Elapsed().ToMilliseconds();
        somme = s;
        return ms;
    }

    // Les deux versions donnent-elles le même quaternion, sur un angle ? q et -q désignent la
    // même rotation : on accepte donc l'égalité au signe près.
    void VerifierLeMemeQuaternion() {
        const float lacet = 37.f;
        const float tangage = 30.f;
        const NkQuatf a = ParMatrices(lacet, tangage);
        const NkQuatf b = ParQuaternions(lacet, tangage);
        std::printf("lacet %g, tangage %g\n", (double)lacet, (double)tangage);
        std::printf("matrices    : x %.6f  y %.6f  z %.6f  w %.6f\n", (double)a.x, (double)a.y, (double)a.z, (double)a.w);
        std::printf("quaternions : x %.6f  y %.6f  z %.6f  w %.6f\n", (double)b.x, (double)b.y, (double)b.z, (double)b.w);
        const float ecartMeme = std::fabs(a.x - b.x) + std::fabs(a.y - b.y) + std::fabs(a.z - b.z) + std::fabs(a.w - b.w);
        const float ecartOppose = std::fabs(a.x + b.x) + std::fabs(a.y + b.y) + std::fabs(a.z + b.z) + std::fabs(a.w + b.w);
        if (ecartMeme < 1e-5f) {
            std::printf("meme quaternion (ecart %.2e)\n", (double)ecartMeme);
        } else if (ecartOppose < 1e-5f) {
            std::printf("meme rotation, signe oppose (ecart %.2e)\n", (double)ecartOppose);
        } else {
            std::printf("DIFFERENTS (ecart %.2e)\n", (double)ecartMeme);
        }
    }

    // Les mesures : trois pour chaque version, imprimées une par ligne.
    void MesurerLeCout() {
        VerifierLeMemeQuaternion();
        double somme = 0.0;
        for (int k = 0; k < MESURES; ++k) {
            const double ms = MesurerMatrices(somme);
            std::printf("matrices : %.3f ms (somme %.4f)\n", ms, somme);
        }
        for (int k = 0; k < MESURES; ++k) {
            const double ms = MesurerQuaternions(somme);
            std::printf("quaternions : %.3f ms (somme %.4f)\n", ms, somme);
        }
        std::fflush(stdout);
    }
}

static void ConfigureAppData(NkAppData &d) {
    d.appName = "Ch06Exo7";
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

namespace {
    constexpr float HAUTEUR_SALLE = 2.50f;
    constexpr float EPAISSEUR_MUR = 0.10f;
    constexpr float DEMI_SALLE = 2.0f;
    constexpr float EPAISSEUR_PANNEAU = 0.02f;

    // Un panneau collé à un mur reste à 5 mm de sa face : coplanaires, les deux
    // surfaces se disputeraient l'affichage (le scintillement de l'exercice 22).
    constexpr float JEU_PANNEAU = 0.005f;
    constexpr float FACE_INTERIEURE = DEMI_SALLE - EPAISSEUR_MUR / 2.0f;
    constexpr float PANNEAU_DECALE = FACE_INTERIEURE - EPAISSEUR_PANNEAU / 2.0f - JEU_PANNEAU;

    constexpr float TABLE_LONGUEUR = 1.20f;
    constexpr float TABLE_PROFONDEUR = 0.80f;
    constexpr float TABLE_HAUTEUR = 0.75f;
    constexpr float PLATEAU_EPAISSEUR = 0.05f;
    constexpr float PIED_SECTION = 0.06f;
    constexpr float TABLE_CENTRE_X = 0.60f;
    constexpr float TABLE_CENTRE_Z = 0.70f;
}


// Pose un objet sur le sol (face basse à y = 0). Le cube unitaire est centré :
// son centre doit monter de la moitié de SA hauteur, pas d'une valeur fixe.
NkMat4f PoserAuSol(NkVec3f taille, float x, float z) {
    const NkVec3f centre = {x, taille.y / 2.0f, z};
    // Translation composée avant l'échelle : T * S met d'abord le cube à la taille
    // voulue autour de son centre, puis déplace le résultat. Dans l'autre ordre,
    // l'échelle multiplierait aussi la position.
    return NkMat4f::Translate(centre) * NkMat4f::Scale(taille);
}

// Même chose, mais sur le dessus de la table (à 0,75 m du sol).
NkMat4f PoserSurTable(NkVec3f taille, float x, float z) {
    const NkVec3f centre = {x, TABLE_HAUTEUR + taille.y / 2.0f, z};
    return NkMat4f::Translate(centre) * NkMat4f::Scale(taille);
}

// Soumet un cube déjà placé. La boîte englobante monde se déduit de la matrice :
// cube unitaire transformé, donc plus aucune boîte à écrire à la main.
static void Pave(NkRender3D *r3d, NkMeshHandle cube, const NkMat4f &transform, NkVec3f couleur) {
    NkDrawCall3D dc;
    dc.mesh = cube;
    dc.transform = transform;
    dc.aabb = NkAABB::Unit().Transformed(transform);
    dc.tint = couleur;
    dc.metallic = 0.f;
    dc.roughness = 0.8f;
    r3d->Submit(dc);
}

// Variante de Pave qui reçoit un matériau : c'est lui qui porte la texture.
static void PaveTexture(NkRender3D *r3d, NkMeshHandle cube, const NkMat4f &transform,
                                NkMatInstHandle materiau) {
    NkDrawCall3D dc;
    dc.mesh = cube;
    dc.material = materiau;
    dc.transform = transform;
    dc.aabb = NkAABB::Unit().Transformed(transform);
    dc.metallic = 0.f;
    dc.roughness = 0.8f;
    r3d->Submit(dc);
}

// Tout ce qui repose sur le sol passe par PoserAuSol.
static void SubmitObjetsAuSol(NkRender3D *r3d, NkMeshHandle cube) {
    const NkVec3f couleurMur = {0.85f, 0.82f, 0.75f};

    // Murs : fond, devant, gauche, droite. Fond et devant dépassent de 0,10 m
    // pour fermer les angles ; les côtés s'arrêtent contre eux.
    Pave(r3d, cube, PoserAuSol({4.10f, HAUTEUR_SALLE, EPAISSEUR_MUR}, 0.f, -DEMI_SALLE), couleurMur);
    Pave(r3d, cube, PoserAuSol({4.10f, HAUTEUR_SALLE, EPAISSEUR_MUR}, 0.f, DEMI_SALLE), couleurMur);
    Pave(r3d, cube, PoserAuSol({EPAISSEUR_MUR, HAUTEUR_SALLE, 3.90f}, -DEMI_SALLE, 0.f), couleurMur);
    Pave(r3d, cube, PoserAuSol({EPAISSEUR_MUR, HAUTEUR_SALLE, 3.90f}, DEMI_SALLE, 0.f), couleurMur);

    // Porte : 0,90 m x 2,00 m, posée au sol contre le mur du fond.
    Pave(r3d, cube, PoserAuSol({0.90f, 2.00f, EPAISSEUR_PANNEAU}, -0.70f, -PANNEAU_DECALE),
                {0.35f, 0.16f, 0.07f});

    // Pieds de la table : de y = 0 jusqu'au dessous du plateau.
    const float pieds = TABLE_HAUTEUR - PLATEAU_EPAISSEUR;
    const float xGauche = TABLE_CENTRE_X - TABLE_LONGUEUR / 2.0f + PIED_SECTION / 2.0f;
    const float xDroite = TABLE_CENTRE_X + TABLE_LONGUEUR / 2.0f - PIED_SECTION / 2.0f;
    const float zHaut = TABLE_CENTRE_Z - TABLE_PROFONDEUR / 2.0f + PIED_SECTION / 2.0f;
    const float zBas = TABLE_CENTRE_Z + TABLE_PROFONDEUR / 2.0f - PIED_SECTION / 2.0f;
    const NkVec3f taillePied = {PIED_SECTION, pieds, PIED_SECTION};
    const NkVec3f couleurPied = {0.32f, 0.16f, 0.08f};
    Pave(r3d, cube, PoserAuSol(taillePied, xGauche, zHaut), couleurPied);
    Pave(r3d, cube, PoserAuSol(taillePied, xGauche, zBas), couleurPied);
    Pave(r3d, cube, PoserAuSol(taillePied, xDroite, zHaut), couleurPied);
    Pave(r3d, cube, PoserAuSol(taillePied, xDroite, zBas), couleurPied);

    // Le reste : une armoire haute contre le mur de droite, un tapis plat.
    Pave(r3d, cube, PoserAuSol({0.40f, 1.80f, 0.60f}, 1.74f, -1.00f), {0.45f, 0.30f, 0.18f});
    Pave(r3d, cube, PoserAuSol({1.60f, 0.02f, 1.20f}, -0.90f, -0.20f), {0.60f, 0.15f, 0.15f});
}

// Objets posés sur le plateau, avec PoserSurTable.
static void SubmitObjetsSurTable(NkRender3D *r3d, NkMeshHandle cube) {
    Pave(r3d, cube, PoserSurTable({0.28f, 0.04f, 0.20f}, 0.40f, 0.70f), {0.15f, 0.25f, 0.60f});
    Pave(r3d, cube, PoserSurTable({0.12f, 0.25f, 0.12f}, 0.90f, 0.60f), {0.90f, 0.90f, 0.85f});
}

// Ce qui ne repose PAS sur le sol : le plateau (porté par les pieds), les
// fenêtres (à 1,40 m) et le sol lui-même. Ils gardent leur calcul direct.
static void SubmitHorsSol(NkRender3D *r3d, NkMeshHandle cube, NkMeshHandle plan) {
    NkDrawCall3D sol;
    sol.mesh = plan;
    sol.transform = NkMat4f::Scale({2.05f, 1.f, 2.05f});  // le plan va de -1 à +1
    sol.aabb = {{-2.05f, 0.f, -2.05f}, {2.05f, 0.f, 2.05f}};
    sol.castShadow = false;  // récepteur d'ombres, pas projeteur
    sol.tint = {0.35f, 0.35f, 0.37f};
    sol.metallic = 0.f;
    sol.roughness = 0.9f;
    r3d->Submit(sol);

    const float centrePlateau = TABLE_HAUTEUR - PLATEAU_EPAISSEUR / 2.0f;
    Pave(r3d, cube,
                NkMat4f::Translate({TABLE_CENTRE_X, centrePlateau, TABLE_CENTRE_Z}) *
                    NkMat4f::Scale({TABLE_LONGUEUR, PLATEAU_EPAISSEUR, TABLE_PROFONDEUR}),
                {0.48f, 0.25f, 0.12f});

    for (const float z : {-0.90f, 0.90f}) {
        Pave(r3d, cube,
                    NkMat4f::Translate({-PANNEAU_DECALE, 1.40f, z}) *
                        NkMat4f::Scale({EPAISSEUR_PANNEAU, 1.00f, 1.20f}),
                    {0.20f, 0.65f, 0.90f});
    }
}

int nkmain(const NkEntryState &state) {
    (void)state;

    // Les mesures : au démarrage, avant d'ouvrir la fenêtre.
    MesurerLeCout();

    float lacetRad = 0.f;

    // ── 2) Fenêtre ────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title = "Ch06 Exo7 - le cout mesure (Q quitte)";
    winCfg.width = 1280;
    winCfg.height = 720;
    winCfg.centered = true;
    winCfg.resizable = true;

    NkWindow window(winCfg);
    if (!window.IsValid()) {
        logger.Error("[Ch06Exo7] Creation fenetre KO");
        return 1;
    }

    // ── 3) Périphérique GPU ───────────────────────────────────────────────────
    NkDeviceInitInfo devInfo{};
    devInfo.surface = window.GetSurfaceDesc();
    devInfo.width = (uint32)window.GetSize().width;
    devInfo.height = (uint32)window.GetSize().height;

    NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInfo);
    if (!device || !device->IsValid()) {
        logger.Error("[Ch06Exo7] Creation device KO");
        window.Close();
        return 2;
    }

    // ── 4) Renderer : dessin de maillages + ombres ────────────────────────────
    NkRendererConfig cfg = NkRendererConfig::ForGame(devInfo.api, devInfo.width, devInfo.height);
    cfg.subsystems = NK_SS_RENDER3D | NK_SS_SHADOW;

    NkRenderer *renderer = NkRenderer::Create(device, cfg);
    if (!renderer || !renderer->Initialize()) {
        logger.Error("[Ch06Exo7] Init renderer KO");
        NkRenderer::Destroy(renderer);
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 3;
    }

    NkRender3D *r3d = renderer->GetRender3D();
    if (!r3d) {
        logger.Error("[Ch06Exo7] NK_SS_RENDER3D absent : rien ne sera dessine");
        NkRenderer::Destroy(renderer);
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 4;
    }

    NkVirtualShadowMaps *shadow = renderer->GetShadow();
    if (shadow) {
        shadow->GetConfig().autoFitDirectional = true;
    }
    r3d->SetIBLStrength(0.5f);

    NkMeshHandle meshCube = renderer->GetMeshSystem()->GetCube();
    NkMeshHandle meshPlan = renderer->GetMeshSystem()->GetPlane();

    // ── 5) La tête : position à hauteur d'yeux ; l'accumulateur de la souris ───
    const NkVec3f positionTete = {-0.70f, 1.60f, 1.00f};
    int accumX = 0;   // comptes du capteur reçus depuis la dernière image

    // ── 6) Événements ─────────────────────────────────────────────────────────
    bool running = true;
    uint32 W = devInfo.width;
    uint32 H = devInfo.height;
    NkEventSystem &events = NkEvents();

    // À chaque événement brut : on AJOUTE. Rien n'est lu ni vidé ici.
    events.AddEventCallback<NkMouseRawEvent>([&](NkMouseRawEvent *e) { accumX += (int)e->GetDeltaX(); });
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *e) {
        if (e->GetKey() == NkKey::NK_Q) {
            running = false;
        }
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent *e) {
        const uint32 w = (uint32)e->GetWidth();
        const uint32 h = (uint32)e->GetHeight();
        if (w > 0 && h > 0 && (w != W || h != H)) {
            W = w;
            H = h;
            renderer->OnResize(w, h);
        }
    });

    // ── 7) Boucle principale ──────────────────────────────────────────────────
    while (running && window.IsOpen()) {
        events.PollEvents();
        if (!running) {
            break;
        }

        if (!renderer->BeginFrame()) {
            continue;  // fenêtre minimisée
        }

        // Une fois par image : on lit l'accumulation, puis on la vide. La souris tourne la tête
        // vers la droite quand on la bouge vers la droite (un lacet positif tourne à gauche).
        lacetRad -= (float)accumX * SENSIBILITE;
        accumX = 0;

        // Caméra : le regard ET le haut viennent du quaternion.
        const NkQuatf q = ParQuaternions(lacetRad * 180.f / PI_FLOAT, TANGAGE_DEG);
        const NkVec3f regard = q * NkVec3f(0.f, 0.f, -1.f);
        const NkVec3f haut = q * NkVec3f(0.f, 1.f, 0.f);

        NkCamera3DData camData;
        camData.up = haut;
        camData.fovY = 70.f;
        camData.aspect = (float32)W / (float32)H;
        camData.nearPlane = 0.05f;
        camData.farPlane = 200.f;
        NkCamera3D cam(camData);
        cam.SetPosition(positionTete);
        cam.SetTarget({positionTete.x + regard.x, positionTete.y + regard.y, positionTete.z + regard.z});

        NkSceneContext sctx;
        sctx.camera = cam;

        NkLightDesc sun;
        sun.type = NkLightType::NK_DIRECTIONAL;
        sun.color = {1.f, 0.95f, 0.85f};
        sun.direction = {-0.4f, -1.f, -0.3f};
        sun.intensity = 3.f;
        sun.castShadow = true;
        sun.shadowStatic = false;
        sctx.lights.PushBack(sun);

        r3d->BeginScene(sctx);
        SubmitObjetsAuSol(r3d, meshCube);
        SubmitObjetsSurTable(r3d, meshCube);
        SubmitHorsSol(r3d, meshCube, meshPlan);

        renderer->Present();
        renderer->EndFrame();
    }

    // ── 8) Fermeture propre (ordre inverse de la création) ────────────────────
    device->WaitIdle();
    NkRenderer::Destroy(renderer);
    NkDeviceFactory::Destroy(device);
    window.Close();
    return 0;
}
