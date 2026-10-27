// =============================================================================
// Chapitre 6, exercice 10 — la projection à la main.
//
// Au démarrage de la salle, avant d'ouvrir la fenêtre, le programme écrit la matrice de
// projection décentrée (Projection : quatre angles en radians, signés, et les plans), à partir
// de la matrice nulle, sans appeler celles du moteur, puis la compare :
//   - épreuve symétrique : 70 degrés verticalement, rapport 1280/720, plans à 0,05 et 100 m
//     (les réglages de la caméra de la salle), contre NkMat4f::Perspective ;
//   - épreuve quest : l'œil gauche d'un Quest 2 (gauche -52, droite 45, haut 48, bas -50 degrés),
//     contre NkCamera3D::SetFovAsym puis GetProj().
// Chaque ligne d'écarts est la valeur absolue de la différence des coefficients mat[0][r] à mat[3][r]
// de la ligne r ; les huit lignes et le plus grand écart de chaque épreuve sont imprimés et écrits
// dans ecarts.txt. La salle s'ouvre ensuite : Q quitte.
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
#include "NKMath/NKMath.h"
#include "NKMath/NkAngle.h"
#include "NKMath/NkMat.h"
#include "NKMath/NkQuat.h"
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

// ── La projection décentrée, écrite à la main ────────────────────────────────
namespace {
    constexpr float32 PI_FLOAT = 3.14159265358979323846f;

    // La matrice de projection décentrée. Les quatre angles sont en radians et signés :
    // gauche et bas sont négatifs. NkMat4f range ses coefficients par colonne : mat[colonne][ligne].
    NkMat4f Projection(float32 gauche, float32 droite, float32 haut, float32 bas, float32 nearPlane, float32 farPlane) {
        const float32 tanLeft = math::NkTan(gauche);
        const float32 tanRight = math::NkTan(droite);
        const float32 tanUp = math::NkTan(haut);
        const float32 tanDown = math::NkTan(bas);
        const float32 tanWidth = tanRight - tanLeft;
        const float32 tanHeight = tanUp - tanDown;

        NkMat4f result = NkMat4f::Zero();   // la matrice NULLE, pas l'identité
        result.mat[0][0] = 2.f / tanWidth;
        result.mat[1][1] = 2.f / tanHeight;
        // Le décentrage : la troisième colonne, multipliée par z de la vue. Nuls en symétrique.
        result.mat[2][0] = (tanRight + tanLeft) / tanWidth;
        result.mat[2][1] = (tanUp + tanDown) / tanHeight;
        result.mat[2][3] = -1.f;
        // La profondeur, entre -1 et 1 : les mêmes coefficients que NkMat4f::Perspective.
        result.mat[2][2] = (farPlane + nearPlane) / (nearPlane - farPlane);
        result.mat[3][2] = 2.f * farPlane * nearPlane / (nearPlane - farPlane);
        return result;
    }

    // L'écart d'un coefficient : la valeur absolue de la différence.
    float32 Ecart(float32 a, float32 b) {
        const float32 d = a - b;
        return d < 0.f ? -d : d;
    }

    // Compare deux matrices, imprime et écrit les quatre lignes d'écarts (ligne r : mat[0][r] à mat[3][r]),
    // puis renvoie le plus grand écart.
    float32 ComparerLesMatrices(std::FILE *fichier, const char *nom, const NkMat4f &mienne, const NkMat4f &moteur) {
        float32 plusGrand = 0.f;
        for (int ligne = 0; ligne < 4; ++ligne) {
            float32 e[4];
            for (int colonne = 0; colonne < 4; ++colonne) {
                e[colonne] = Ecart(mienne.mat[colonne][ligne], moteur.mat[colonne][ligne]);
                if (e[colonne] > plusGrand) {
                    plusGrand = e[colonne];
                }
            }
            std::printf("%s, ligne %d : %.3e %.3e %.3e %.3e\n", nom, ligne, (double)e[0], (double)e[1], (double)e[2],
                        (double)e[3]);
            if (fichier) {
                std::fprintf(fichier, "%s, ligne %d : %.3e %.3e %.3e %.3e\n", nom, ligne, (double)e[0], (double)e[1],
                             (double)e[2], (double)e[3]);
            }
        }
        return plusGrand;
    }

    void ImprimerUneMatrice(const char *nom, const NkMat4f &m) {
        std::printf("%s (ligne r : mat[0][r] mat[1][r] mat[2][r] mat[3][r]) :\n", nom);
        for (int ligne = 0; ligne < 4; ++ligne) {
            std::printf("  %12.7f %12.7f %12.7f %12.7f\n", (double)m.mat[0][ligne], (double)m.mat[1][ligne],
                        (double)m.mat[2][ligne], (double)m.mat[3][ligne]);
        }
    }

    // Les deux épreuves : écrit ecarts.txt (huit lignes d'écarts, puis le plus grand écart de chaque épreuve).
    void LesEcarts() {
        std::FILE *fichier = std::fopen("ecarts.txt", "w");

        // Épreuve symétrique : les réglages de la caméra de la salle, 70 degrés verticalement,
        // rapport 1280/720, plans à 0,05 et 100 mètres. Les quatre angles s'en déduisent :
        // demi-hauteur = 35 degrés ; tan(demi-largeur) = rapport x tan(35 degrés).
        const float32 nearPlane = 0.05f;
        const float32 farPlane = 100.f;
        const float32 rapport = 1280.f / 720.f;
        const float32 demiHauteur = 35.f * PI_FLOAT / 180.f;
        const float32 demiLargeur = std::atan(rapport * math::NkTan(demiHauteur));
        const NkMat4f mienneSym = Projection(-demiLargeur, demiLargeur, demiHauteur, -demiHauteur, nearPlane, farPlane);
        const NkMat4f moteurSym = NkMat4f::Perspective(NkAngle(70.f), rapport, nearPlane, farPlane);
        ImprimerUneMatrice("symetrique, la mienne", mienneSym);
        ImprimerUneMatrice("symetrique, Perspective", moteurSym);
        const float32 plusGrandSym = ComparerLesMatrices(fichier, "symetrique", mienneSym, moteurSym);

        // Épreuve quest : l'œil gauche d'un Quest 2, gauche -52, droite 45, haut 48, bas -50 degrés,
        // comparée à la caméra du moteur (SetFovAsym : radians, signés).
        const float32 degRad = PI_FLOAT / 180.f;
        const float32 gauche = -52.f * degRad;
        const float32 droite = 45.f * degRad;
        const float32 haut = 48.f * degRad;
        const float32 bas = -50.f * degRad;
        const NkMat4f mienneQuest = Projection(gauche, droite, haut, bas, nearPlane, farPlane);
        NkCamera3D cam;
        cam.SetNearFar(nearPlane, farPlane);
        cam.SetFovAsym(gauche, droite, haut, bas);
        const NkMat4f moteurQuest = cam.GetProj();
        ImprimerUneMatrice("quest, la mienne", mienneQuest);
        ImprimerUneMatrice("quest, camera du moteur", moteurQuest);
        const float32 plusGrandQuest = ComparerLesMatrices(fichier, "quest", mienneQuest, moteurQuest);

        std::printf("symetrique : plus grand ecart %.3e\nquest : plus grand ecart %.3e\n", (double)plusGrandSym,
                    (double)plusGrandQuest);
        if (fichier) {
            std::fprintf(fichier, "symetrique : plus grand ecart %.3e\nquest : plus grand ecart %.3e\n",
                         (double)plusGrandSym, (double)plusGrandQuest);
            std::fclose(fichier);
        }
        std::fflush(stdout);
    }
}

static void ConfigureAppData(NkAppData &d) {
    d.appName = "Ch06Exo10";
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

    // Les écarts de projection : au démarrage, avant d'ouvrir la fenêtre.
    LesEcarts();

    // La caméra de la salle : à hauteur d'yeux, le regard vers -Z.
    const NkVec3f positionTete = {0.30f, 1.70f, 1.20f};

    // ── 2) Fenêtre ────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title = "Ch06 Exo10 - la projection a la main (Q quitte)";
    winCfg.width = 1280;
    winCfg.height = 720;
    winCfg.centered = true;
    winCfg.resizable = true;

    NkWindow window(winCfg);
    if (!window.IsValid()) {
        logger.Error("[Ch06Exo10] Creation fenetre KO");
        return 1;
    }

    // ── 3) Périphérique GPU ───────────────────────────────────────────────────
    NkDeviceInitInfo devInfo{};
    devInfo.surface = window.GetSurfaceDesc();
    devInfo.width = (uint32)window.GetSize().width;
    devInfo.height = (uint32)window.GetSize().height;

    NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInfo);
    if (!device || !device->IsValid()) {
        logger.Error("[Ch06Exo10] Creation device KO");
        window.Close();
        return 2;
    }

    // ── 4) Renderer : dessin de maillages + ombres ────────────────────────────
    NkRendererConfig cfg = NkRendererConfig::ForGame(devInfo.api, devInfo.width, devInfo.height);
    cfg.subsystems = NK_SS_RENDER3D | NK_SS_SHADOW;

    NkRenderer *renderer = NkRenderer::Create(device, cfg);
    if (!renderer || !renderer->Initialize()) {
        logger.Error("[Ch06Exo10] Init renderer KO");
        NkRenderer::Destroy(renderer);
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 3;
    }

    NkRender3D *r3d = renderer->GetRender3D();
    if (!r3d) {
        logger.Error("[Ch06Exo10] NK_SS_RENDER3D absent : rien ne sera dessine");
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

    // ── 6) Événements ─────────────────────────────────────────────────────────
    bool running = true;
    uint32 W = devInfo.width;
    uint32 H = devInfo.height;
    NkEventSystem &events = NkEvents();

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

        // Caméra : le regard ET le haut viennent du quaternion.
        const NkQuatf q = NkQuatf::Identity();
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
