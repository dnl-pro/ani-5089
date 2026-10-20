// =============================================================================
// Chapitre 5, exercice 7 — un panneau au-dessus de la porte.
//
// Le mot « Sortie » est écrit avec une police EMBARQUÉE dans le moteur (Inter,
// 32 pixels), sans aucun fichier de police :
//   1. une atlas de police est construite (NkFontAtlas) ;
//   2. une image est créée, et chaque lettre y est copiée DEPUIS L'ATLAS, à partir
//      des coordonnées que donne le moteur (x0..y1 en pixels, u0..v1 dans l'atlas) ;
//   3. l'image est enregistrée dans panneau.png, puis posée comme texture sur un
//      pavé de 20 cm de haut, au-dessus de la porte.
//
// Touches : 1 = caméra à trois mètres du panneau, 2 = caméra à cinquante
// centimètres. Échap ou fermeture de la fenêtre : quitter.
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
#include "NKFont/NkFont.h"
#include "NKFont/Embedded/NkFontEmbedded.h"
#include "NKImage/Core/NkImage.h"

#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Materials/NkMaterial.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Shadow/NkVirtualShadowMaps.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

static void ConfigureAppData(NkAppData &d) {
    d.appName = "Ch05Exo7";
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

// Le panneau : fichier écrit par le programme, puis chargé comme texture.
static const char *FICHIER_PANNEAU = "panneau.png";
static const char *FICHIER_PANNEAU_POSE = "panneau_pose.png";

// Réglages du panneau. 0xRRGGBBAA : texte blanc sur un fond vert de sortie.
constexpr uint32 COULEUR_FOND = 0x1E8449FF;
constexpr float TEXTE_R = 255.f, TEXTE_G = 255.f, TEXTE_B = 255.f;
constexpr float TAILLE_POLICE = 32.f;
constexpr float HAUTEUR_PANNEAU = 0.20f;

// Sens de la texture sur le pavé. Si le mot apparaît en miroir dans la fenêtre,
// passer RETOURNER_HORIZONTALEMENT à false ; s'il est à l'envers, passer
// RETOURNER_VERTICALEMENT à true.
constexpr bool RETOURNER_HORIZONTALEMENT = true;
constexpr bool RETOURNER_VERTICALEMENT = false;

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

// Construit l'image du mot, en lisant chaque lettre DANS l'atlas.
// Rend une image invalide si la police ou l'atlas échoue. Remplit les tailles.
static NkImage ConstruireImagePanneau(const char *mot, int &atlasL, int &atlasH) {
    NkFontAtlas atlas;
    NkFont *police = NkFontEmbedded::AddToAtlas(atlas, NkEmbeddedFontId::Inter, TAILLE_POLICE);
    if (!police) {
        logger.Error("[Ch05Exo7] Police embarquee Inter indisponible");
        return NkImage();
    }
    if (!atlas.Build()) {
        logger.Error("[Ch05Exo7] Construction de l'atlas KO");
        return NkImage();
    }
    atlasL = atlas.texWidth;
    atlasH = atlas.texHeight;

    // Un octet par pixel : combien la lettre couvre ce pixel, de 0 à 255.
    uint8 *couverture = nullptr;
    int32 texL = 0;
    int32 texH = 0;
    atlas.GetTexDataAsAlpha8(&couverture, &texL, &texH);
    if (!couverture || texL <= 0 || texH <= 0) {
        logger.Error("[Ch05Exo7] Pixels de l'atlas introuvables");
        return NkImage();
    }

    // Taille de l'image : la somme des advanceX, sur la hauteur ascent - descent.
    float somme = 0.f;
    for (const char *c = mot; *c; ++c) {
        const NkFontGlyph *g = police->FindGlyph((NkFontCodepoint)(unsigned char)*c);
        if (!g) {
            logger.Error("[Ch05Exo7] Lettre absente de l'atlas : {0}", *c);
            return NkImage();
        }
        somme += g->advanceX;
    }
    const int largeur = (int)std::ceil(somme);
    const int hauteur = (int)std::ceil(police->ascent - police->descent);
    const int ligneDeBase = (int)std::ceil(police->ascent);  // à ascent pixels du haut
    NkImage image = NkImage::Create((uint32)largeur, (uint32)hauteur, 4, COULEUR_FOND);
    if (!image.IsValid()) {
        logger.Error("[Ch05Exo7] Creation de l'image KO");
        return NkImage();
    }

    // Lit la couverture de l'atlas à une position réelle (en pixels), en mélangeant
    // les quatre pixels voisins : l'atlas est surechantillonné, ce mélange évite
    // de sauter un pixel sur deux.
    auto lireCouverture = [&](float ax, float ay) {
        ax -= 0.5f;
        ay -= 0.5f;
        int x0 = (int)std::floor(ax);
        int y0 = (int)std::floor(ay);
        const float fx = ax - (float)x0;
        const float fy = ay - (float)y0;
        auto pixel = [&](int x, int y) {
            if (x < 0) x = 0;
            if (y < 0) y = 0;
            if (x >= texL) x = texL - 1;
            if (y >= texH) y = texH - 1;
            return (float)couverture[y * texL + x];
        };
        const float haut = pixel(x0, y0) * (1.f - fx) + pixel(x0 + 1, y0) * fx;
        const float bas = pixel(x0, y0 + 1) * (1.f - fx) + pixel(x0 + 1, y0 + 1) * fx;
        return haut * (1.f - fy) + bas * fy;
    };

    float curseur = 0.f;
    for (const char *c = mot; *c; ++c) {
        const NkFontGlyph *g = police->FindGlyph((NkFontCodepoint)(unsigned char)*c);
        // La lettre occupe ce rectangle de l'image : curseur + x0 .. curseur + x1
        // en largeur, ligne de base + y0 .. ligne de base + y1 en hauteur.
        const float gauche = curseur + g->x0;
        const float droite = curseur + g->x1;
        const float haut = (float)ligneDeBase + g->y0;
        const float bas = (float)ligneDeBase + g->y1;
        const int pxDebut = (int)std::floor(gauche);
        const int pxFin = (int)std::ceil(droite);
        const int pyDebut = (int)std::floor(haut);
        const int pyFin = (int)std::ceil(bas);
        for (int py = pyDebut; py < pyFin; ++py) {
            for (int px = pxDebut; px < pxFin; ++px) {
                if (px < 0 || py < 0 || px >= largeur || py >= hauteur) {
                    continue;
                }
                // Fraction du rectangle de la lettre, au centre du pixel, puis
                // la même fraction dans le rectangle (u0..u1, v0..v1) de l'atlas.
                const float fx = ((float)px + 0.5f - gauche) / (droite - gauche);
                const float fy = ((float)py + 0.5f - haut) / (bas - haut);
                if (fx < 0.f || fx > 1.f || fy < 0.f || fy > 1.f) {
                    continue;
                }
                const float u = g->u0 + fx * (g->u1 - g->u0);
                const float v = g->v0 + fy * (g->v1 - g->v0);
                const float alpha = lireCouverture(u * (float)texL, v * (float)texH) / 255.f;

                // Mélange : alpha de la couleur du texte, le reste de ce qui est déjà là.
                const auto dessous = image.GetPixel(px, py);
                math::NkColor melange;
                melange.r = (uint8)(dessous.r * (1.f - alpha) + TEXTE_R * alpha + 0.5f);
                melange.g = (uint8)(dessous.g * (1.f - alpha) + TEXTE_G * alpha + 0.5f);
                melange.b = (uint8)(dessous.b * (1.f - alpha) + TEXTE_B * alpha + 0.5f);
                melange.a = 255;
                image.SetPixel(px, py, melange);
            }
        }
        curseur += g->advanceX;
    }
    return image;
}

// Le panneau : un pavé de 20 cm de haut, au-dessus de la porte, contre le mur.
static void SubmitPanneau(NkRender3D *r3d, NkMeshHandle cube, NkMatInstHandle materiau, float largeur) {
    // Centre : x = celui de la porte (-0,70), au-dessus de son sommet (2,00 m)
    // avec 5 cm de jeu ; z = celui des autres panneaux, décollé de 5 mm du mur.
    const NkMat4f transform = NkMat4f::Translate({-0.70f, 2.00f + 0.05f + HAUTEUR_PANNEAU / 2.0f, -PANNEAU_DECALE}) *
                              NkMat4f::Scale({largeur, HAUTEUR_PANNEAU, EPAISSEUR_PANNEAU});
    PaveTexture(r3d, cube, transform, materiau);
}

int nkmain(const NkEntryState &state) {
    (void)state;

    // ── 2) Fenêtre ────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title = "Ch05 Exo7 - touches 1 trois metres / 2 cinquante cm";
    winCfg.width = 1280;
    winCfg.height = 720;
    winCfg.centered = true;
    winCfg.resizable = true;

    NkWindow window(winCfg);
    if (!window.IsValid()) {
        logger.Error("[Ch05Exo7] Creation fenetre KO");
        return 1;
    }

    // ── 3) Périphérique GPU ───────────────────────────────────────────────────
    NkDeviceInitInfo devInfo{};
    devInfo.surface = window.GetSurfaceDesc();
    devInfo.width = (uint32)window.GetSize().width;
    devInfo.height = (uint32)window.GetSize().height;

    NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInfo);
    if (!device || !device->IsValid()) {
        logger.Error("[Ch05Exo7] Creation device KO");
        window.Close();
        return 2;
    }

    // ── 4) Renderer : dessin de maillages + ombres ────────────────────────────
    NkRendererConfig cfg = NkRendererConfig::ForGame(devInfo.api, devInfo.width, devInfo.height);
    cfg.subsystems = NK_SS_RENDER3D | NK_SS_SHADOW;

    NkRenderer *renderer = NkRenderer::Create(device, cfg);
    if (!renderer || !renderer->Initialize()) {
        logger.Error("[Ch05Exo7] Init renderer KO");
        NkRenderer::Destroy(renderer);
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 3;
    }

    NkRender3D *r3d = renderer->GetRender3D();
    if (!r3d) {
        logger.Error("[Ch05Exo7] NK_SS_RENDER3D absent : rien ne sera dessine");
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

    // ── 5) L'image du panneau, puis sa texture : UNE fois, avant la boucle ────
    int atlasL = 0;
    int atlasH = 0;
    NkImage panneau = ConstruireImagePanneau("Sortie", atlasL, atlasH);
    bool panneauPret = panneau.IsValid();
    float largeurPanneau = HAUTEUR_PANNEAU;
    NkMatInstHandle materiauPanneau;
    if (panneauPret) {
        std::printf("atlas : %d x %d\n", atlasL, atlasH);
        std::printf("image : %d x %d\n", (int)panneau.Width(), (int)panneau.Height());
        logger.Info("[Ch05Exo7] atlas : {0} x {1}", atlasL, atlasH);
        logger.Info("[Ch05Exo7] image : {0} x {1}", (int)panneau.Width(), (int)panneau.Height());
        // panneau.png : le mot à l'endroit, tel qu'il se lit.
        panneauPret = panneau.SavePNG(FICHIER_PANNEAU);
        // Les faces du cube lisent leurs UV à l'envers de gauche à droite : vu de
        // face, le mot serait en miroir. La texture du pavé est donc la même
        // image retournée horizontalement, enregistrée à part. panneau.png, lui,
        // reste lisible.
        NkImage pose = panneau.Copy();
        if (RETOURNER_HORIZONTALEMENT) {
            pose.FlipHorizontal();
        }
        if (RETOURNER_VERTICALEMENT) {
            pose.FlipVertical();
        }
        panneauPret = panneauPret && pose.SavePNG(FICHIER_PANNEAU_POSE);
        // Largeur du pavé : celle qui garde les proportions de l'image.
        largeurPanneau = HAUTEUR_PANNEAU * (float)panneau.Width() / (float)panneau.Height();
    }
    if (panneauPret) {
        NkTexHandle texture = renderer->GetTextures()->Load(FICHIER_PANNEAU_POSE);
        NkMaterial *materiau = NkMaterial::Create(renderer->GetMaterials(), NkMaterialType::NK_PBR_METALLIC);
        materiau->SetAlbedoMap(texture);
        materiauPanneau = materiau->GetInstHandle();
    } else {
        logger.Error("[Ch05Exo7] Panneau non construit : le mur reste sans panneau");
    }

    // ── 6) Événements ─────────────────────────────────────────────────────────
    bool running = true;
    int camera = 1;  // 1 = trois mètres, 2 = cinquante centimètres
    uint32 W = devInfo.width;
    uint32 H = devInfo.height;
    NkEventSystem &events = NkEvents();

    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *e) {
        if (e->GetKey() == NkKey::NK_ESCAPE) {
            running = false;
        }
        if (e->GetKey() == NkKey::NK_NUM1) {
            camera = 1;
            logger.Info("[Ch05Exo7] Camera : trois metres");
        }
        if (e->GetKey() == NkKey::NK_NUM2) {
            camera = 2;
            logger.Info("[Ch05Exo7] Camera : cinquante centimetres");
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
        // La caméra regarde le centre du panneau dans les deux cas.
        const NkVec3f centrePanneau = {-0.70f, 2.00f + 0.05f + HAUTEUR_PANNEAU / 2.0f, -PANNEAU_DECALE};
        if (camera == 2) {
            cam.SetPosition({-0.70f, 2.20f, -1.50f});  // cinquante centimètres
        } else {
            cam.SetPosition({-0.70f, 1.70f, 1.00f});  // trois mètres
        }
        cam.SetTarget(centrePanneau);

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
        if (panneauPret) {
            SubmitPanneau(r3d, meshCube, materiauPanneau, largeurPanneau);
        }
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
