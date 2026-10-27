// =============================================================================
// Chapitre 6, exercice 8 — les deux poses et l'axe du monde.
//
// Au démarrage de la salle, avant d'ouvrir la fenêtre, le programme place la tête en
// (0,30 ; 1,70 ; 1,20) et déduit les deux yeux (écart des pupilles : 63 mm) de deux façons :
//   - tete  : le long de la droite de la tête, qui est +X tourné par l'orientation de la tête ;
//   - monde : le long de l'axe x du monde, toujours (1, 0, 0).
// Trois poses : neutre (Identity), roulis 30 (RotateZ 30°) et lacet 90 (RotateY 90°).
// Les six positions (mètres, quatre décimales) et les trois écarts de l'œil gauche (millimètres)
// sont imprimés et écrits dans poses.txt.
// NkQuatf::Right() n'est pas employé : il tourne -X, la droite d'un repère où l'avant est +Z,
// alors que dans cette salle l'avant est -Z et la droite +X.
// La salle s'ouvre ensuite, caméra à la place de la tête : 1, 2, 3 changent de pose, Q quitte.
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

// ── Les deux yeux, de deux façons ────────────────────────────────────────────
namespace {
    constexpr float IPD = 0.063f;   // l'écart entre les pupilles : 63 mm, la valeur du simulateur

    struct Pose {
        NkVec3f position;
        NkQuatf orientation;
    };

    // La droite de la tête : l'image, par son orientation, de la droite de la salle, +X.
    // (NkQuatf::Right() tourne -X, la droite d'un repère où l'avant est +Z : ce n'est pas celle de
    // cette salle, où l'avant est -Z. On tourne donc +X nous-mêmes.)
    NkVec3f Droite(const Pose &pose) {
        return pose.orientation * NkVec3f(1.f, 0.f, 0.f);
    }

    // Les deux yeux, décalés de la moitié de l'écart le long de l'axe donné.
    void DeduireLesYeux(const Pose &poseTete, const NkVec3f &axe, Pose &poseOeilGauche, Pose &poseOeilDroit) {
        const float demiEcart = IPD * 0.5f;
        poseOeilGauche.orientation = poseTete.orientation;
        poseOeilDroit.orientation = poseTete.orientation;
        poseOeilGauche.position = poseTete.position - axe * demiEcart;
        poseOeilDroit.position = poseTete.position + axe * demiEcart;
    }

    // Une position arrondie à quatre décimales, comme elle sera écrite : l'écart est calculé sur
    // ces valeurs, pour qu'on puisse le retrouver à la main à partir des lignes du fichier.
    float Arrondi(float v) {
        const float r = std::round(v * 10000.f) / 10000.f;
        return r == 0.f ? 0.f : r;   // pas de « -0.0000 »
    }
    NkVec3f Arrondi(const NkVec3f &v) {
        return NkVec3f(Arrondi(v.x), Arrondi(v.y), Arrondi(v.z));
    }

    struct PoseNommee {
        const char *nom;
        NkQuatf orientation;
    };

    const PoseNommee POSES[3] = {
        {"neutre", NkQuatf::Identity()},
        {"roulis 30", NkQuatf::RotateZ(NkAngle(30.f))},
        {"lacet 90", NkQuatf::RotateY(NkAngle(90.f))},
    };

    void EcrireUneLigne(std::FILE *fichier, const char *nom, const char *methode, const Pose &g, const Pose &d) {
        const NkVec3f a = Arrondi(g.position);
        const NkVec3f b = Arrondi(d.position);
        std::printf("%s, %s : G (%.4f, %.4f, %.4f) D (%.4f, %.4f, %.4f)\n", nom, methode, (double)a.x, (double)a.y,
                    (double)a.z, (double)b.x, (double)b.y, (double)b.z);
        if (fichier) {
            std::fprintf(fichier, "%s, %s : G (%.4f, %.4f, %.4f) D (%.4f, %.4f, %.4f)\n", nom, methode, (double)a.x,
                         (double)a.y, (double)a.z, (double)b.x, (double)b.y, (double)b.z);
        }
    }

    // Les neuf lignes de poses.txt : six positions, puis les trois écarts de l'œil gauche, en millimètres.
    void CalculerLesPoses(const NkVec3f &positionTete) {
        std::FILE *fichier = std::fopen("poses.txt", "w");
        float ecartsMm[3] = {0.f, 0.f, 0.f};
        for (int i = 0; i < 3; ++i) {
            const Pose tete = {positionTete, POSES[i].orientation};
            Pose gaucheTete, droitTete, gaucheMonde, droitMonde;
            DeduireLesYeux(tete, Droite(tete), gaucheTete, droitTete);                  // le long de la droite de la tête
            DeduireLesYeux(tete, NkVec3f(1.f, 0.f, 0.f), gaucheMonde, droitMonde);       // le long de l'axe x du monde
            EcrireUneLigne(fichier, POSES[i].nom, "tete", gaucheTete, droitTete);
            EcrireUneLigne(fichier, POSES[i].nom, "monde", gaucheMonde, droitMonde);
            const NkVec3f ecart = Arrondi(gaucheTete.position) - Arrondi(gaucheMonde.position);
            ecartsMm[i] = std::sqrt(ecart.x * ecart.x + ecart.y * ecart.y + ecart.z * ecart.z) * 1000.f;   // m -> mm
        }
        for (int i = 0; i < 3; ++i) {
            std::printf("%s : ecart %.1f mm\n", POSES[i].nom, (double)ecartsMm[i]);
            if (fichier) {
                std::fprintf(fichier, "%s : ecart %.1f mm\n", POSES[i].nom, (double)ecartsMm[i]);
            }
        }
        if (fichier) {
            std::fclose(fichier);
        }
        std::fflush(stdout);
    }
}

static void ConfigureAppData(NkAppData &d) {
    d.appName = "Ch06Exo8";
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

    // La tête, à la place de la caméra de la salle.
    const NkVec3f positionTete = {0.30f, 1.70f, 1.20f};

    // Les poses : au démarrage, avant d'ouvrir la fenêtre.
    CalculerLesPoses(positionTete);

    int poseChoisie = 0;   // 1, 2, 3 changent la pose de la caméra

    // ── 2) Fenêtre ────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title = "Ch06 Exo8 - les deux poses et l'axe du monde (1 neutre, 2 roulis 30, 3 lacet 90, Q quitte)";
    winCfg.width = 1280;
    winCfg.height = 720;
    winCfg.centered = true;
    winCfg.resizable = true;

    NkWindow window(winCfg);
    if (!window.IsValid()) {
        logger.Error("[Ch06Exo8] Creation fenetre KO");
        return 1;
    }

    // ── 3) Périphérique GPU ───────────────────────────────────────────────────
    NkDeviceInitInfo devInfo{};
    devInfo.surface = window.GetSurfaceDesc();
    devInfo.width = (uint32)window.GetSize().width;
    devInfo.height = (uint32)window.GetSize().height;

    NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInfo);
    if (!device || !device->IsValid()) {
        logger.Error("[Ch06Exo8] Creation device KO");
        window.Close();
        return 2;
    }

    // ── 4) Renderer : dessin de maillages + ombres ────────────────────────────
    NkRendererConfig cfg = NkRendererConfig::ForGame(devInfo.api, devInfo.width, devInfo.height);
    cfg.subsystems = NK_SS_RENDER3D | NK_SS_SHADOW;

    NkRenderer *renderer = NkRenderer::Create(device, cfg);
    if (!renderer || !renderer->Initialize()) {
        logger.Error("[Ch06Exo8] Init renderer KO");
        NkRenderer::Destroy(renderer);
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 3;
    }

    NkRender3D *r3d = renderer->GetRender3D();
    if (!r3d) {
        logger.Error("[Ch06Exo8] NK_SS_RENDER3D absent : rien ne sera dessine");
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
        if (e->GetKey() == NkKey::NK_NUM1) {
            poseChoisie = 0;
        }
        if (e->GetKey() == NkKey::NK_NUM2) {
            poseChoisie = 1;
        }
        if (e->GetKey() == NkKey::NK_NUM3) {
            poseChoisie = 2;
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
        const NkQuatf q = POSES[poseChoisie].orientation;
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
