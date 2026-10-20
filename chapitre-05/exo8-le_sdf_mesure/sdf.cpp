// =============================================================================
// Chapitre 5, exercice 8 — le SDF mesuré.
//
// Le mot « Sortie » est fabriqué DEUX fois, à huit fois la taille des lettres de
// l'atlas, avec Inter à 32 pixels :
//   - une fois avec un atlas ordinaire (la valeur lue est la couverture) ;
//   - une fois avec un atlas SDF (la valeur lue est une distance au bord, et le
//     bord est à 0,5), en appliquant sur le processeur la règle du nuanceur
//     kNkFontFragSDF du moteur : alpha = smoothstep(0.5 - s, 0.5 + s, distance).
// Un troisième atlas, ordinaire, à 128 pixels, sert à la comparaison en mémoire.
// Les trois tailles d'atlas sont écrites par le programme dans atlas.txt.
//
// Touches : 1 = caméra à trois mètres, 2 = cinquante centimètres,
//           3 = panneau ordinaire, 4 = panneau SDF. Échap : quitter.
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
    d.appName = "Ch05Exo8";
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

// Fichiers écrits par le programme : les deux images (à l'endroit, comme on les
// lit) et leurs versions retournées, qui servent de textures sur le pavé.
static const char *FICHIER_ATLAS_TXT = "atlas.txt";
static const char *FICHIER_ORDINAIRE = "ordinaire.png";
static const char *FICHIER_SDF = "sdf.png";
static const char *FICHIER_ORDINAIRE_POSE = "ordinaire_pose.png";
static const char *FICHIER_SDF_POSE = "sdf_pose.png";

// Réglages. 0xRRGGBBAA : texte blanc sur un fond vert de sortie.
constexpr uint32 COULEUR_FOND = 0x1E8449FF;
constexpr float TEXTE_R = 255.f, TEXTE_G = 255.f, TEXTE_B = 255.f;
constexpr float TAILLE_POLICE = 32.f;
constexpr float TAILLE_POLICE_GRANDE = 128.f;
constexpr int ECHELLE = 8;               // l'image fait 8 fois la taille des lettres de l'atlas
constexpr int ETALEMENT_SDF = 6;         // atlas.sdfSpread
constexpr float HAUTEUR_PANNEAU = 0.20f;

// Douceur du bord SDF. Essais sur l'image à huit fois la taille (nombre de
// pixels de bord intermédiaires, ni fond ni lettre) : s = 0,005 -> 458 (bord
// tout ou rien, escalier net) ; 0,01 -> 750 ; 0,02 -> 4084 ; 0,04 -> 5635 ;
// 0,08 -> 13870 (bord flou, presque autant que l'atlas ordinaire : 29077).
// s = 0,04 étale le bord sur 0,08 de distance, soit un peu plus d'un pixel
// d'atlas : l'escalier de l'atlas est adouci sans que le bord devienne flou.
constexpr float S_BORD = 0.04f;

// Sens de la texture sur le pavé (voir l'exercice 7) : le cube lit ses UV à
// l'envers de gauche à droite, on retourne donc l'image horizontalement.
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


// --- La règle du nuanceur ----------------------------------------------------

// Recopiée de l'énoncé : t va de 0 à 1 entre a et b, puis t*t*(3-2t) adoucit les extrémités.
static float smoothstep(float a, float b, float x) {
    float t = (x - a) / (b - a);
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;
    return t * t * (3.f - 2.f * t);
}

// --- Atlas ---------------------------------------------------------------------

// Construit un atlas avec Inter à la taille donnée, ordinaire ou SDF.
// Rend la police (ou nullptr). Les tailles sont lues dans atlas.texWidth/texHeight.
static NkFont *ConstruireAtlas(NkFontAtlas &atlas, float taille, bool sdf) {
    if (sdf) {
        atlas.sdfMode = true;
        atlas.sdfSpread = ETALEMENT_SDF;
    }
    NkFont *police = NkFontEmbedded::AddToAtlas(atlas, NkEmbeddedFontId::Inter, taille);
    if (!police) {
        logger.Error("[Ch05Exo8] Police embarquee Inter indisponible");
        return nullptr;
    }
    if (!atlas.Build()) {
        logger.Error("[Ch05Exo8] Construction de l'atlas KO");
        return nullptr;
    }
    return police;
}

// Lit une valeur de l'atlas à une position réelle (en pixels), en mélangeant les
// quatre pixels voisins (interpolation bilinéaire). Le centre du pixel (i, j) est
// en (i + 0,5 ; j + 0,5), d'où le décalage de 0,5.
static float LireBilineaire(const uint8 *octets, int largeur, int hauteur, float ax, float ay) {
    ax -= 0.5f;
    ay -= 0.5f;
    const int x0 = (int)std::floor(ax);
    const int y0 = (int)std::floor(ay);
    const float fx = ax - (float)x0;
    const float fy = ay - (float)y0;
    auto pixel = [&](int x, int y) {
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x >= largeur) x = largeur - 1;
        if (y >= hauteur) y = hauteur - 1;
        return (float)octets[y * largeur + x];
    };
    const float haut = pixel(x0, y0) * (1.f - fx) + pixel(x0 + 1, y0) * fx;
    const float bas = pixel(x0, y0 + 1) * (1.f - fx) + pixel(x0 + 1, y0 + 1) * fx;
    return haut * (1.f - fy) + bas * fy;
}

// Fabrique l'image du mot à ECHELLE fois la taille des lettres de l'atlas.
// sdf = false : la valeur lue / 255 est la couverture.
// sdf = true  : la valeur lue / 255 est une distance (bord à 0,5), et le
//               nuanceur du moteur donne l'opacité : smoothstep(0,5 - s ; 0,5 + s ; distance).
// Les deux appels donnent des images de MÊME taille : seule la méthode change.
static NkImage ConstruireImage(const char *mot, NkFont *police, NkFontAtlas &atlas, bool sdf, float s) {
    uint8 *octets = nullptr;
    int32 texL = 0;
    int32 texH = 0;
    atlas.GetTexDataAsAlpha8(&octets, &texL, &texH);
    if (!octets || texL <= 0 || texH <= 0) {
        logger.Error("[Ch05Exo8] Pixels de l'atlas introuvables");
        return NkImage();
    }

    float somme = 0.f;
    for (const char *c = mot; *c; ++c) {
        const NkFontGlyph *g = police->FindGlyph((NkFontCodepoint)(unsigned char)*c);
        if (!g) {
            logger.Error("[Ch05Exo8] Lettre absente de l'atlas : {0}", *c);
            return NkImage();
        }
        somme += g->advanceX;
    }
    // Largeur : ECHELLE x la somme des advanceX. Hauteur : ECHELLE x (ascent - descent).
    const int largeur = ECHELLE * (int)std::ceil(somme);
    const int hauteur = ECHELLE * (int)std::ceil(police->ascent - police->descent);
    const float ligneDeBase = (float)ECHELLE * std::ceil(police->ascent);
    NkImage image = NkImage::Create((uint32)largeur, (uint32)hauteur, 4, COULEUR_FOND);
    if (!image.IsValid()) {
        logger.Error("[Ch05Exo8] Creation de l'image KO");
        return NkImage();
    }

    float curseur = 0.f;  // en pixels de police ; l'image est ECHELLE fois plus grande
    for (const char *c = mot; *c; ++c) {
        const NkFontGlyph *g = police->FindGlyph((NkFontCodepoint)(unsigned char)*c);
        const float gauche = ECHELLE * (curseur + g->x0);
        const float droite = ECHELLE * (curseur + g->x1);
        const float haut = ligneDeBase + ECHELLE * g->y0;
        const float bas = ligneDeBase + ECHELLE * g->y1;
        for (int py = (int)std::floor(haut); py < (int)std::ceil(bas); ++py) {
            for (int px = (int)std::floor(gauche); px < (int)std::ceil(droite); ++px) {
                if (px < 0 || py < 0 || px >= largeur || py >= hauteur) {
                    continue;
                }
                const float fx = ((float)px + 0.5f - gauche) / (droite - gauche);
                const float fy = ((float)py + 0.5f - haut) / (bas - haut);
                if (fx < 0.f || fx > 1.f || fy < 0.f || fy > 1.f) {
                    continue;
                }
                const float u = g->u0 + fx * (g->u1 - g->u0);
                const float v = g->v0 + fy * (g->v1 - g->v0);
                const float valeur = LireBilineaire(octets, texL, texH, u * (float)texL, v * (float)texH);

                float alpha;
                if (sdf) {
                    const float distance = valeur / 255.f;   // 0,5 = le bord ; en dessous = dehors
                    alpha = smoothstep(0.5f - s, 0.5f + s, distance);
                } else {
                    alpha = valeur / 255.f;                  // la couverture
                }

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

// Écrit les trois tailles relevées dans atlas.txt, sans retouche.
static bool EcrireAtlasTxt(int ordL, int ordH, int sdfL, int sdfH, int grandL, int grandH) {
    std::FILE *f = std::fopen(FICHIER_ATLAS_TXT, "w");
    if (!f) {
        logger.Error("[Ch05Exo8] Impossible d'ecrire atlas.txt");
        return false;
    }
    std::fprintf(f, "ordinaire 32 : %d x %d\n", ordL, ordH);
    std::fprintf(f, "sdf 32 : %d x %d\n", sdfL, sdfH);
    std::fprintf(f, "ordinaire 128 : %d x %d\n", grandL, grandH);
    std::fclose(f);
    return true;
}

// Une image prête à poser : enregistrée à l'endroit, puis retournée pour le pavé.
static bool EnregistrerImage(NkImage &image, const char *fichierDroit, const char *fichierPose) {
    if (!image.IsValid() || !image.SavePNG(fichierDroit)) {
        return false;
    }
    NkImage pose = image.Copy();
    if (RETOURNER_HORIZONTALEMENT) {
        pose.FlipHorizontal();
    }
    if (RETOURNER_VERTICALEMENT) {
        pose.FlipVertical();
    }
    return pose.SavePNG(fichierPose);
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
    winCfg.title = "Ch05 Exo8 - 1 trois metres / 2 cinquante cm / 3 ordinaire / 4 sdf";
    winCfg.width = 1280;
    winCfg.height = 720;
    winCfg.centered = true;
    winCfg.resizable = true;

    NkWindow window(winCfg);
    if (!window.IsValid()) {
        logger.Error("[Ch05Exo8] Creation fenetre KO");
        return 1;
    }

    // ── 3) Périphérique GPU ───────────────────────────────────────────────────
    NkDeviceInitInfo devInfo{};
    devInfo.surface = window.GetSurfaceDesc();
    devInfo.width = (uint32)window.GetSize().width;
    devInfo.height = (uint32)window.GetSize().height;

    NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInfo);
    if (!device || !device->IsValid()) {
        logger.Error("[Ch05Exo8] Creation device KO");
        window.Close();
        return 2;
    }

    // ── 4) Renderer : dessin de maillages + ombres ────────────────────────────
    NkRendererConfig cfg = NkRendererConfig::ForGame(devInfo.api, devInfo.width, devInfo.height);
    cfg.subsystems = NK_SS_RENDER3D | NK_SS_SHADOW;

    NkRenderer *renderer = NkRenderer::Create(device, cfg);
    if (!renderer || !renderer->Initialize()) {
        logger.Error("[Ch05Exo8] Init renderer KO");
        NkRenderer::Destroy(renderer);
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 3;
    }

    NkRender3D *r3d = renderer->GetRender3D();
    if (!r3d) {
        logger.Error("[Ch05Exo8] NK_SS_RENDER3D absent : rien ne sera dessine");
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

    // ── 5) Les trois atlas, les deux images, les deux textures : UNE fois ─────
    NkFontAtlas atlasOrdinaire;
    NkFontAtlas atlasSdf;
    NkFont *policeOrdinaire = ConstruireAtlas(atlasOrdinaire, TAILLE_POLICE, false);
    NkFont *policeSdf = ConstruireAtlas(atlasSdf, TAILLE_POLICE, true);
    int grandL = 0;
    int grandH = 0;
    {
        // Le troisième atlas (ordinaire, 128 pixels) ne sert qu'à relever sa taille.
        NkFontAtlas atlasGrand;
        if (ConstruireAtlas(atlasGrand, TAILLE_POLICE_GRANDE, false)) {
            grandL = atlasGrand.texWidth;
            grandH = atlasGrand.texHeight;
        }
    }

    bool panneauxPrets = false;
    float largeurPanneau = HAUTEUR_PANNEAU;
    NkMatInstHandle materiauOrdinaire;
    NkMatInstHandle materiauSdf;
    if (policeOrdinaire && policeSdf && grandL > 0) {
        EcrireAtlasTxt(atlasOrdinaire.texWidth, atlasOrdinaire.texHeight, atlasSdf.texWidth, atlasSdf.texHeight,
                       grandL, grandH);
        NkImage ordinaire = ConstruireImage("Sortie", policeOrdinaire, atlasOrdinaire, false, 0.f);
        NkImage sdf = ConstruireImage("Sortie", policeSdf, atlasSdf, true, S_BORD);
        if (ordinaire.IsValid() && sdf.IsValid() && ordinaire.Width() == sdf.Width() &&
            ordinaire.Height() == sdf.Height()) {
            std::printf("ordinaire 32 : %d x %d\n", atlasOrdinaire.texWidth, atlasOrdinaire.texHeight);
            std::printf("sdf 32 : %d x %d\n", atlasSdf.texWidth, atlasSdf.texHeight);
            std::printf("ordinaire 128 : %d x %d\n", grandL, grandH);
            std::printf("image : %d x %d (les deux)\n", (int)ordinaire.Width(), (int)ordinaire.Height());
            panneauxPrets = EnregistrerImage(ordinaire, FICHIER_ORDINAIRE, FICHIER_ORDINAIRE_POSE) &&
                            EnregistrerImage(sdf, FICHIER_SDF, FICHIER_SDF_POSE);
            // Largeur du pavé : celle qui garde les proportions de l'image.
            largeurPanneau = HAUTEUR_PANNEAU * (float)ordinaire.Width() / (float)ordinaire.Height();
        } else {
            logger.Error("[Ch05Exo8] Images absentes ou de tailles differentes");
        }
    }
    if (panneauxPrets) {
        NkTexHandle texOrdinaire = renderer->GetTextures()->Load(FICHIER_ORDINAIRE_POSE);
        NkMaterial *matOrdinaire = NkMaterial::Create(renderer->GetMaterials(), NkMaterialType::NK_PBR_METALLIC);
        matOrdinaire->SetAlbedoMap(texOrdinaire);
        materiauOrdinaire = matOrdinaire->GetInstHandle();

        NkTexHandle texSdf = renderer->GetTextures()->Load(FICHIER_SDF_POSE);
        NkMaterial *matSdf = NkMaterial::Create(renderer->GetMaterials(), NkMaterialType::NK_PBR_METALLIC);
        matSdf->SetAlbedoMap(texSdf);
        materiauSdf = matSdf->GetInstHandle();
    } else {
        logger.Error("[Ch05Exo8] Panneaux non construits : le mur reste sans panneau");
    }

    // ── 6) Événements ─────────────────────────────────────────────────────────
    bool running = true;
    int camera = 1;  // 1 = trois mètres, 2 = cinquante centimètres
    bool afficheSdf = false;  // 3 = panneau ordinaire, 4 = panneau SDF
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
            logger.Info("[Ch05Exo8] Camera : trois metres");
        }
        if (e->GetKey() == NkKey::NK_NUM2) {
            camera = 2;
            logger.Info("[Ch05Exo8] Camera : cinquante centimetres");
        }
        if (e->GetKey() == NkKey::NK_NUM3) {
            afficheSdf = false;
            logger.Info("[Ch05Exo8] Panneau : ordinaire");
        }
        if (e->GetKey() == NkKey::NK_NUM4) {
            afficheSdf = true;
            logger.Info("[Ch05Exo8] Panneau : sdf");
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
        if (panneauxPrets) {
            SubmitPanneau(r3d, meshCube, afficheSdf ? materiauSdf : materiauOrdinaire, largeurPanneau);
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
