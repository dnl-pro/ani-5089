// =============================================================================
// Chapitre 6, exercice 6 — le cône du mauvais ordre.
//
// La caméra de la salle est pilotée par un quaternion : le regard et le haut
// viennent de lui.
//   cd.target = cd.position + q * (0, 0, -1)      cd.up = q * (0, 1, 0)
// La souris ne pilote que le lacet (les événements bruts sont accumulés, lus une
// fois par image, puis vidés) ; le tangage est fixé à 30 degrés. Deux ordres :
//   bon     : q = qLacet * qTangage  (le tangage d'abord, puis le lacet : le geste d'une tête) ;
//   mauvais : q = qTangage * qLacet  (le lacet d'abord, puis le tangage autour de l'axe x du monde).
// Touches : O change d'ordre, Q quitte.
// Variables d'environnement :
//   MASALLE_LACET = degrés : fige le lacet à cette valeur et coupe la souris ;
//   MASALLE_ORDRE = mauvais : démarre dans le mauvais ordre (sinon, le bon).
// Au démarrage, avant la fenêtre : la hauteur du regard (son angle au-dessus de
// l'horizon) aux lacets 0, 45, 90, 135 et 180, dans les deux ordres, est imprimée
// et écrite dans hauteurs.txt.
// =============================================================================
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkAngle.h"
#include "NKMath/NkQuat.h"
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

// ── L'orientation, dans les deux ordres ─────────────────────────────────────
namespace {
    constexpr float TANGAGE_DEG = 30.f;      // fixé : seule la souris pilote le lacet
    constexpr float SENSIBILITE = 0.002f;    // radians par compte du capteur
    constexpr float PI_FLOAT = 3.14159265358979323846f;

    // L'orientation de la tête : bon ordre = lacet * tangage, mauvais ordre = tangage * lacet.
    NkQuatf Orientation(float lacetDeg, bool bonOrdre) {
        const NkQuatf qLacet = NkQuatf::RotateY(NkAngle(lacetDeg));
        const NkQuatf qTangage = NkQuatf::RotateX(NkAngle(TANGAGE_DEG));
        return bonOrdre ? (qLacet * qTangage) : (qTangage * qLacet);
    }

    // La hauteur du regard : son angle au-dessus de l'horizon, en degrés (asin de la composante y).
    float HauteurDeg(float lacetDeg, bool bonOrdre) {
        const NkVec3f regard = Orientation(lacetDeg, bonOrdre) * NkVec3f(0.f, 0.f, -1.f);
        return std::asin(regard.y) * 180.f / PI_FLOAT;
    }

    // « -0.0 » s'imprimerait mal : un nombre minuscule s'écrit 0.
    float Propre(float v) {
        return std::fabs(v) < 0.05f ? 0.f : v;
    }

    // Les dix hauteurs, imprimées et écrites dans hauteurs.txt.
    void EcrireLesHauteurs() {
        std::FILE *fichier = std::fopen("hauteurs.txt", "w");
        const float lacets[5] = {0.f, 45.f, 90.f, 135.f, 180.f};
        for (const bool bon : {true, false}) {
            for (const float lacet : lacets) {
                const float h = Propre(HauteurDeg(lacet, bon));
                std::printf("%s, lacet %g : hauteur %.1f deg\n", bon ? "lacet*tangage" : "tangage*lacet", (double)lacet,
                            (double)h);
                if (fichier) {
                    std::fprintf(fichier, "%s, lacet %g : hauteur %.1f deg\n", bon ? "lacet*tangage" : "tangage*lacet",
                                 (double)lacet, (double)h);
                }
            }
        }
        if (fichier) {
            std::fclose(fichier);
        }
    }
}

static void ConfigureAppData(NkAppData &d) {
    d.appName = "Ch06Exo6";
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

    // Les dix hauteurs : au démarrage, avant d'ouvrir la fenêtre.
    EcrireLesHauteurs();

    // La pose figée : MASALLE_LACET donne le lacet en degrés et coupe la souris.
    const char *envLacet = std::getenv("MASALLE_LACET");
    const bool lacetFige = (envLacet != nullptr);
    float lacetRad = lacetFige ? (float)std::atof(envLacet) * PI_FLOAT / 180.f : 0.f;
    const char *envOrdre = std::getenv("MASALLE_ORDRE");
    bool bonOrdre = !(envOrdre && std::strcmp(envOrdre, "mauvais") == 0);

    // ── 2) Fenêtre ────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title = "Ch06 Exo6 - le cone du mauvais ordre (O ordre, Q quitte)";
    winCfg.width = 1280;
    winCfg.height = 720;
    winCfg.centered = true;
    winCfg.resizable = true;

    NkWindow window(winCfg);
    if (!window.IsValid()) {
        logger.Error("[Ch06Exo6] Creation fenetre KO");
        return 1;
    }

    // ── 3) Périphérique GPU ───────────────────────────────────────────────────
    NkDeviceInitInfo devInfo{};
    devInfo.surface = window.GetSurfaceDesc();
    devInfo.width = (uint32)window.GetSize().width;
    devInfo.height = (uint32)window.GetSize().height;

    NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInfo);
    if (!device || !device->IsValid()) {
        logger.Error("[Ch06Exo6] Creation device KO");
        window.Close();
        return 2;
    }

    // ── 4) Renderer : dessin de maillages + ombres ────────────────────────────
    NkRendererConfig cfg = NkRendererConfig::ForGame(devInfo.api, devInfo.width, devInfo.height);
    cfg.subsystems = NK_SS_RENDER3D | NK_SS_SHADOW;

    NkRenderer *renderer = NkRenderer::Create(device, cfg);
    if (!renderer || !renderer->Initialize()) {
        logger.Error("[Ch06Exo6] Init renderer KO");
        NkRenderer::Destroy(renderer);
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 3;
    }

    NkRender3D *r3d = renderer->GetRender3D();
    if (!r3d) {
        logger.Error("[Ch06Exo6] NK_SS_RENDER3D absent : rien ne sera dessine");
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
        if (e->GetKey() == NkKey::NK_O) {
            bonOrdre = !bonOrdre;
            logger.Info("[Ch06Exo6] ordre : {0}", bonOrdre ? "lacet*tangage (bon)" : "tangage*lacet (mauvais)");
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
        if (!lacetFige) {
            lacetRad -= (float)accumX * SENSIBILITE;
        }
        accumX = 0;

        // Caméra : le regard ET le haut viennent du quaternion.
        const NkQuatf q = Orientation(lacetRad * 180.f / PI_FLOAT, bonOrdre);
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
