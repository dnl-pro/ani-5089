// =============================================================================
// Chapitre 6, exercice 1 — le pointeur capturé.
//
// Dans la salle, la souris sera une tête : aucune flèche ne doit flotter dans le
// monde, ni partir sur un autre écran. Tout ce qui touche au pointeur est ici :
//   - juste après l'ouverture de la fenêtre : le pointeur est caché et capturé,
//     puis confiné à la zone client (ClipMouseToClient) ;
//   - Échap rend tout (pointeur visible, libre, confinement levé) ;
//   - Espace reprend tout ;
//   - Q quitte ;
//   - quelle que soit la sortie (Q, Échap, croix de la fenêtre), le pointeur est
//     rendu avant la fin du programme.
//
// Essais : la variable d'environnement POINTEUR_ESSAI sert aux expériences de
// essais.txt. Sans elle, le programme est la version corrigée.
//   POINTEUR_ESSAI=1 : CaptureMouse seul (pointeur caché et capturé, rien d'autre) ;
//   POINTEUR_ESSAI=2 : version d'avant correction : confinement, mais chaque appui
//                      appelle ShowMouse sans compter, et rien n'est rendu à la fin.
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

#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Materials/NkMaterial.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Shadow/NkVirtualShadowMaps.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

// ── Le pointeur ─────────────────────────────────────────────────────────────
// ShowMouse est un COMPTEUR sous Windows (voir son commentaire dans NkWindow.h) :
// deux appels à false demandent deux appels à true pour revenir. On retient donc
// l'état dans une variable, et on n'appelle le système que quand il change.
namespace {
    // Essai demandé par POINTEUR_ESSAI : 0 = version corrigée, 1 = capture seule, 2 = avant correction.
    int LireEssai() {
        const char *v = std::getenv("POINTEUR_ESSAI");
        return v ? std::atoi(v) : 0;
    }

    // Reprend le pointeur : caché, capturé, confiné à la fenêtre.
    void ReprendrePointeur(NkWindow &fenetre, bool &repris, int essai) {
        if (essai == 2) {
            // Avant correction : chaque appui rappelle ShowMouse(false), sans compter.
            fenetre.ShowMouse(false);
            fenetre.CaptureMouse(true);
            fenetre.ClipMouseToClient(true);
            repris = true;
            return;
        }
        if (repris) {
            return;  // déjà repris : un second appel à ShowMouse(false) creuserait le compteur
        }
        fenetre.ShowMouse(false);
        fenetre.CaptureMouse(true);
        if (essai != 1) {
            fenetre.ClipMouseToClient(true);
        }
        repris = true;
    }

    // Rend le pointeur : visible, libre, confinement levé.
    void RendrePointeur(NkWindow &fenetre, bool &repris, int essai) {
        if (essai != 2 && !repris) {
            return;  // déjà rendu : un ShowMouse(true) de trop ferait monter le compteur au-dessus de zéro
        }
        fenetre.ShowMouse(true);
        fenetre.CaptureMouse(false);
        fenetre.ClipMouseToClient(false);
        repris = false;
    }
}

static void ConfigureAppData(NkAppData &d) {
    d.appName = "Ch06Exo1";
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

    // ── 2) Fenêtre ────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title = "Ch06 Exo1 - Echap rend le pointeur, Espace le reprend, Q quitte";
    winCfg.width = 1280;
    winCfg.height = 720;
    winCfg.centered = true;
    winCfg.resizable = true;

    NkWindow window(winCfg);
    if (!window.IsValid()) {
        logger.Error("[Ch06Exo1] Creation fenetre KO");
        return 1;
    }

    // ── 3) Périphérique GPU ───────────────────────────────────────────────────
    NkDeviceInitInfo devInfo{};
    devInfo.surface = window.GetSurfaceDesc();
    devInfo.width = (uint32)window.GetSize().width;
    devInfo.height = (uint32)window.GetSize().height;

    NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInfo);
    if (!device || !device->IsValid()) {
        logger.Error("[Ch06Exo1] Creation device KO");
        window.Close();
        return 2;
    }

    // ── 4) Renderer : dessin de maillages + ombres ────────────────────────────
    NkRendererConfig cfg = NkRendererConfig::ForGame(devInfo.api, devInfo.width, devInfo.height);
    cfg.subsystems = NK_SS_RENDER3D | NK_SS_SHADOW;

    NkRenderer *renderer = NkRenderer::Create(device, cfg);
    if (!renderer || !renderer->Initialize()) {
        logger.Error("[Ch06Exo1] Init renderer KO");
        NkRenderer::Destroy(renderer);
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 3;
    }

    NkRender3D *r3d = renderer->GetRender3D();
    if (!r3d) {
        logger.Error("[Ch06Exo1] NK_SS_RENDER3D absent : rien ne sera dessine");
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

    // ── 5) Le pointeur : caché et capturé juste après l'ouverture de la fenêtre ─
    const int essai = LireEssai();
    bool pointeurRepris = false;
    ReprendrePointeur(window, pointeurRepris, essai);

    // ── 6) Événements ─────────────────────────────────────────────────────────
    bool running = true;
    uint32 W = devInfo.width;
    uint32 H = devInfo.height;
    NkEventSystem &events = NkEvents();

    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *e) {
        if (e->GetKey() == NkKey::NK_ESCAPE) {
            RendrePointeur(window, pointeurRepris, essai);
        }
        if (e->GetKey() == NkKey::NK_SPACE) {
            ReprendrePointeur(window, pointeurRepris, essai);
        }
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

        // Caméra fixe, tournée vers le mur du fond.
        NkCamera3DData camData;
        camData.up = {0.f, 1.f, 0.f};
        camData.fovY = 70.f;
        camData.aspect = (float32)W / (float32)H;
        camData.nearPlane = 0.05f;
        camData.farPlane = 200.f;
        NkCamera3D cam(camData);
        cam.SetPosition({-0.70f, 1.70f, 1.00f});
        cam.SetTarget({-0.70f, 1.20f, -PANNEAU_DECALE});

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
    // Quelle que soit la sortie (Q, Échap puis croix, croix seule), le pointeur est
    // rendu ici. Seul l'essai 2 (avant correction) ne le fait pas.
    if (essai != 2) {
        RendrePointeur(window, pointeurRepris, essai);
    }
    device->WaitIdle();
    NkRenderer::Destroy(renderer);
    NkDeviceFactory::Destroy(device);
    window.Close();
    return 0;
}
