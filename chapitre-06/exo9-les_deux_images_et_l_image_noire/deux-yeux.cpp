// =============================================================================
// Chapitre 6, exercice 9 — les deux images et l'image noire.
//
// La salle est dessinée deux fois, une par œil, dans deux cibles hors écran de 640 x 720
// (chacune avec son renderer de salle), puis affichée côte à côte dans la fenêtre de 1280 x 720.
// La tête est en (0,30 ; 1,70 ; 1,20), droite, le regard vers -Z ; les yeux sont décalés de
// 63 mm le long de la droite de la tête. Seule la position de la caméra change d'un œil à l'autre.
//   - image 30 : les deux cibles sont capturées (oeil-gauche.png, oeil-droit.png) ;
//   - après la boucle, les fichiers sont rechargés (NkImage::Load) et comparés pixel par pixel :
//     l'écart d'un pixel est la plus grande différence de ses trois canaux ; difference.png vaut
//     quatre fois l'écart, borné à 255 (noir = pareil) ;
//   - image 60 : la même chose avec les deux caméras à la place de la tête (vue oubliée), ce qui
//     donne difference-vue-oubliee.png. Les captures provisoires de cette seconde passe sont effacées.
// Les deux lignes de difference.txt sont écrites par le programme. Q quitte avant la fin : rien n'est écrit.
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
#include "NKImage/Core/NkImage.h"

#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Materials/NkMaterial.h"
#include "NKRenderer/Materials/NkMaterialCollection.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
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
}

// ── La comparaison des deux images ───────────────────────────────────────────
namespace {
    constexpr int LARGEUR_OEIL = 640;
    constexpr int HAUTEUR_OEIL = 720;
    constexpr int IMAGE_DE_CAPTURE = 30;   // les deux yeux sont capturés à la même image

    struct Soustraction {
        bool valide = false;
        long long pixels = 0;   // pixels dont l'écart dépasse 1
        long long total = 0;
        int ecartMax = 0;
    };

    int Absolu(int v) {
        return v < 0 ? -v : v;
    }

    // Un pixel est « uniforme » si toute l'image a la même couleur que lui : deux images blanches
    // (ou noires) donneraient une différence nulle sans rien prouver.
    bool ImageUniforme(const NkImage &image) {
        const auto ref = image.GetPixel(0, 0);
        for (int y = 0; y < image.Height(); ++y) {
            for (int x = 0; x < image.Width(); ++x) {
                const auto p = image.GetPixel(x, y);
                if (p.r != ref.r || p.g != ref.g || p.b != ref.b) {
                    return false;
                }
            }
        }
        return true;
    }

    // Recharge les deux fichiers, les compare pixel par pixel, écrit l'image de la différence :
    // chaque pixel vaut quatre fois l'écart (la plus grande différence des trois canaux), borné à 255.
    // Le noir veut dire « pareil ».
    Soustraction Soustraire(const char *fichierGauche, const char *fichierDroit, const char *fichierDifference) {
        Soustraction s;
        NkImage gauche;
        NkImage droit;
        if (!gauche.Load(fichierGauche, 4) || !droit.Load(fichierDroit, 4)) {
            std::printf("[ERREUR] impossible de recharger %s ou %s\n", fichierGauche, fichierDroit);
            return s;
        }
        if (gauche.Width() != LARGEUR_OEIL || gauche.Height() != HAUTEUR_OEIL || droit.Width() != LARGEUR_OEIL ||
            droit.Height() != HAUTEUR_OEIL) {
            std::printf("[ERREUR] tailles inattendues : %dx%d et %dx%d (attendu %dx%d)\n", gauche.Width(),
                        gauche.Height(), droit.Width(), droit.Height(), LARGEUR_OEIL, HAUTEUR_OEIL);
            return s;
        }
        if (ImageUniforme(gauche) || ImageUniforme(droit)) {
            std::printf("[ATTENTION] une des deux images est d'une seule couleur : la différence ne prouve rien.\n");
        }

        NkImage difference = NkImage::Create((uint32)LARGEUR_OEIL, (uint32)HAUTEUR_OEIL, 4);
        for (int y = 0; y < HAUTEUR_OEIL; ++y) {
            for (int x = 0; x < LARGEUR_OEIL; ++x) {
                const auto a = gauche.GetPixel(x, y);
                const auto b = droit.GetPixel(x, y);
                int ecart = Absolu((int)a.r - (int)b.r);
                const int eg = Absolu((int)a.g - (int)b.g);
                const int eb = Absolu((int)a.b - (int)b.b);
                if (eg > ecart) {
                    ecart = eg;
                }
                if (eb > ecart) {
                    ecart = eb;
                }
                if (ecart > 1) {
                    ++s.pixels;
                }
                if (ecart > s.ecartMax) {
                    s.ecartMax = ecart;
                }
                int valeur = ecart * 4;
                if (valeur > 255) {
                    valeur = 255;
                }
                math::NkColor c;
                c.r = (uint8)valeur;
                c.g = (uint8)valeur;
                c.b = (uint8)valeur;
                c.a = 255;
                difference.SetPixel(x, y, c);
                ++s.total;
            }
        }
        s.valide = difference.SavePNG(fichierDifference);
        return s;
    }
}

static void ConfigureAppData(NkAppData &d) {
    d.appName = "Ch06Exo9";
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

    // La tête : à la place de la caméra de la salle, droite, le regard vers -Z.
    const Pose tete = {NkVec3f(0.30f, 1.70f, 1.20f), NkQuatf::Identity()};
    Pose gauche, droit;
    DeduireLesYeux(tete, Droite(tete), gauche, droit);   // le long de la droite de la tête
    Pose posesOeil[2] = {gauche, droit};

    // ── 1) Fenêtre ────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title = "Ch06 Exo9 - les deux images (Q quitte)";
    winCfg.width = 1280;
    winCfg.height = 720;
    winCfg.centered = true;
    winCfg.resizable = false;

    NkWindow window(winCfg);
    if (!window.IsValid()) {
        logger.Error("[Ch06Exo9] Creation fenetre KO");
        return 1;
    }

    // ── 2) Périphérique GPU ───────────────────────────────────────────────────
    NkDeviceInitInfo devInfo{};
    devInfo.surface = window.GetSurfaceDesc();
    devInfo.width = (uint32)window.GetSize().width;
    devInfo.height = (uint32)window.GetSize().height;

    NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInfo);
    if (!device || !device->IsValid()) {
        logger.Error("[Ch06Exo9] Creation device KO");
        window.Close();
        return 2;
    }

    // ── 3) Le renderer de la fenêtre : il possède la frame et compose les deux yeux. ─────
    // Offscreen activé : c'est lui qui crée les deux cibles, que DrawImage doit connaître.
    NkRendererConfig cfgFenetre = NkRendererConfig::For2D(devInfo.api, devInfo.width, devInfo.height);
    cfgFenetre.Enable(NK_SS_OFFSCREEN);
    NkRenderer *rFenetre = NkRenderer::Create(device, cfgFenetre);
    if (!rFenetre) {
        logger.Error("[Ch06Exo9] Renderer de la fenetre KO");
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 3;
    }

    // ── 4) Deux cibles hors écran, un renderer de salle par œil ───────────────
    NkOffscreenTarget *cibles[2] = {nullptr, nullptr};
    NkRenderer *rOeil[2] = {nullptr, nullptr};
    const char *nomsCibles[2] = {"Ch06Exo9_OeilGauche", "Ch06Exo9_OeilDroit"};
    for (int e = 0; e < 2; ++e) {
        NkOffscreenDesc od;
        od.width = (uint32)LARGEUR_OEIL;
        od.height = (uint32)HAUTEUR_OEIL;
        od.hasDepth = true;
        od.hdr = false;
        od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
        od.readable = true;
        od.readback = true;   // pour pouvoir capturer la cible dans un fichier
        od.name = nomsCibles[e];
        cibles[e] = rFenetre->CreateOffscreen(od);
        if (!cibles[e]) {
            logger.Error("[Ch06Exo9] Cible hors ecran KO");
            return 4;
        }

        // Le renderer de salle de cet œil : la 3D et les ombres ; le 2D appartient à la fenêtre.
        NkRendererConfig cfgOeil = NkRendererConfig::ForGame(devInfo.api, (uint32)LARGEUR_OEIL, (uint32)HAUTEUR_OEIL);
        cfgOeil.Disable(NK_SS_RENDER2D);
        cfgOeil.Disable(NK_SS_TEXT);
        cfgOeil.Disable(NK_SS_OVERLAY);
        cfgOeil.Enable(NK_SS_OFFSCREEN);
        rOeil[e] = NkRenderer::Create(device, cfgOeil);
        if (!rOeil[e]) {
            logger.Error("[Ch06Exo9] Renderer de l'oeil KO");
            return 5;
        }
        rOeil[e]->SetFinalColorTarget(rFenetre->GetTextures()->GetRHIHandle(cibles[e]->GetColorResult()));
        // La taille de la cible : sans cette ligne, le graphe rend à la taille de la fenêtre.
        rOeil[e]->SetRenderSizeOverride((uint32)LARGEUR_OEIL, (uint32)HAUTEUR_OEIL);
        if (NkVirtualShadowMaps *shadow = rOeil[e]->GetShadow()) {
            shadow->GetConfig().autoFitDirectional = true;
        }
        if (NkRender3D *r3d = rOeil[e]->GetRender3D()) {
            r3d->SetIBLStrength(0.5f);
        }
    }

    // ── 5) Événements ─────────────────────────────────────────────────────────
    bool running = true;
    NkEventSystem &events = NkEvents();
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *e) {
        if (e->GetKey() == NkKey::NK_Q) {
            running = false;
        }
    });

    // ── 6) Boucle principale ──────────────────────────────────────────────────
    int image = 0;               // numéro de l'image dessinée
    bool decaler = true;         // vrai : les yeux sont décalés ; faux : la vue est « oubliée »
    Soustraction deuxYeux;
    Soustraction vueOubliee;
    bool captureFaite = false;

    while (running && window.IsOpen()) {
        events.PollEvents();
        if (!running) {
            break;
        }
        if (!rFenetre->BeginFrame()) {
            continue;   // fenêtre minimisée
        }
        NkICommandBuffer *cmd = rFenetre->GetCmd();

        // Chaque œil rejoue ce que BeginFrame ferait si ce renderer possédait la frame :
        // sans ResetFrame, le second œil hérite de ce que le premier a laissé.
        for (int e = 0; e < 2; ++e) {
            NkRenderer *r = rOeil[e];
            NkRender3D *r3d = r->GetRender3D();
            r->FlushGraphRebuilds();
            r3d->ResetFrame();
            if (NkMaterialCollection *mc = r->GetMaterialCollection()) {
                mc->Upload();
            }

            // Seule la position de la caméra change d'un œil à l'autre. Vue oubliée : les deux
            // caméras sont à la place de la tête.
            const NkVec3f position = decaler ? posesOeil[e].position : tete.position;
            NkCamera3DData camData;
            camData.up = NkVec3f(0.f, 1.f, 0.f);
            camData.fovY = 70.f;
            camData.aspect = (float32)LARGEUR_OEIL / (float32)HAUTEUR_OEIL;
            camData.nearPlane = 0.05f;
            camData.farPlane = 200.f;
            NkCamera3D cam(camData);
            cam.SetPosition(position);
            cam.SetTarget({position.x, position.y, position.z - 1.f});   // le regard vers -Z

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
            const NkMeshHandle cube = r->GetMeshSystem()->GetCube();
            const NkMeshHandle plan = r->GetMeshSystem()->GetPlane();
            SubmitObjetsAuSol(r3d, cube);
            SubmitObjetsSurTable(r3d, cube);
            SubmitHorsSol(r3d, cube, plan);

            // Le graphe de l'œil s'exécute sur le tampon de commandes de l'écran.
            if (NkRenderGraph *graph = r->GetRenderGraph()) {
                graph->Execute(cmd);
            }
        }

        // Les deux cibles côte à côte : gauche à x = 0, droite à x = 640.
        NkOverlayRenderer *overlay = rFenetre->GetOverlay();
        NkRender2D *r2d = rFenetre->GetRender2D();
        if (overlay && r2d) {
            overlay->BeginOverlay(cmd, devInfo.width, devInfo.height);
            r2d->DrawImage(cibles[0]->GetColorResult(), {0.f, 0.f, (float32)LARGEUR_OEIL, (float32)HAUTEUR_OEIL});
            r2d->DrawImage(cibles[1]->GetColorResult(),
                           {(float32)LARGEUR_OEIL, 0.f, (float32)LARGEUR_OEIL, (float32)HAUTEUR_OEIL});
            overlay->EndOverlay();
        }

        rFenetre->Present();
        rFenetre->EndFrame();
        ++image;

        // À l'image 30 : les deux yeux, capturés à la même image. À l'image 60 : la même chose,
        // les yeux n'étant plus décalés (fichiers provisoires, effacés après la comparaison).
        if (image == IMAGE_DE_CAPTURE || image == 2 * IMAGE_DE_CAPTURE) {
            device->WaitIdle();
            const bool vueOubli = (image == 2 * IMAGE_DE_CAPTURE);
            const char *nomGauche = vueOubli ? "provisoire-gauche.png" : "oeil-gauche.png";
            const char *nomDroit = vueOubli ? "provisoire-droit.png" : "oeil-droit.png";
            const bool okG = cibles[0]->Capture(nomGauche);
            const bool okD = cibles[1]->Capture(nomDroit);
            std::printf("image %d : capture %s / %s (%s, %s)\n", image, nomGauche, nomDroit, okG ? "ok" : "ECHEC",
                        okD ? "ok" : "ECHEC");
            if (vueOubli) {
                captureFaite = okG && okD;
                running = false;
            } else {
                decaler = false;   // la suite : les yeux ne sont plus décalés
            }
        }
    }

    // ── 7) Fermeture propre (avant de relire les fichiers) ────────────────────
    device->WaitIdle();
    for (int e = 0; e < 2; ++e) {
        NkRenderer::Destroy(rOeil[e]);
        rFenetre->DestroyOffscreen(cibles[e]);
    }
    NkRenderer::Destroy(rFenetre);
    NkDeviceFactory::Destroy(device);
    window.Close();

    // ── 8) Les soustractions, faites par le programme ─────────────────────────
    if (!captureFaite) {
        std::printf("[ERREUR] les captures n'ont pas toutes été faites (fenêtre fermée avant l'image %d ?).\n",
                    2 * IMAGE_DE_CAPTURE);
        return 6;
    }
    deuxYeux = Soustraire("oeil-gauche.png", "oeil-droit.png", "difference.png");
    vueOubliee = Soustraire("provisoire-gauche.png", "provisoire-droit.png", "difference-vue-oubliee.png");
    std::remove("provisoire-gauche.png");
    std::remove("provisoire-droit.png");
    if (!deuxYeux.valide || !vueOubliee.valide) {
        return 7;
    }

    std::FILE *fichier = std::fopen("difference.txt", "w");
    char ligne1[128];
    char ligne2[128];
    std::snprintf(ligne1, sizeof(ligne1), "deux yeux : %lld pixels sur %lld, ecart max %d", deuxYeux.pixels,
                  deuxYeux.total, deuxYeux.ecartMax);
    std::snprintf(ligne2, sizeof(ligne2), "vue oubliee : %lld pixels sur %lld, ecart max %d", vueOubliee.pixels,
                  vueOubliee.total, vueOubliee.ecartMax);
    std::printf("%s\n%s\n", ligne1, ligne2);
    if (fichier) {
        std::fprintf(fichier, "%s\n%s\n", ligne1, ligne2);
        std::fclose(fichier);
    }
    return 0;
}
