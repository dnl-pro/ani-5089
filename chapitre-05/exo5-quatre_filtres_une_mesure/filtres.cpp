// =============================================================================
// Chapitre 5, exercice 5 — quatre filtres, une mesure.
//
// Le programme :
//   1. charge une photo et la réduit en 2048 x 2048 avec Resize : c'est la SOURCE ;
//   2. réduit cette source en 256 x 256 avec chacun des quatre filtres du moteur :
//      NK_NEAREST, NK_BILINEAR, NK_BICUBIC, NK_LANCZOS3 ;
//   3. chronomètre chaque filtre avec NkChrono : vingt réductions, on garde la
//      MOYENNE en microsecondes ;
//   4. enregistre le dernier résultat de chaque filtre en PNG ;
//   5. compare chaque résultat à celui du bilinéaire, pixel par pixel, avec
//      GetPixel, et compte les pixels qui diffèrent sur au moins un canal ;
//   6. écrit mesures.txt, une ligne par filtre :  NK_BICUBIC : T us, differences : D
//
// Usage :  filtres [photo]      (par défaut : photo.jpg)
//
// Le chemin de la photo est relatif au dossier D'OÙ LE PROGRAMME EST LANCÉ.
// À compiler en Release : en Debug, on chronométrerait surtout le compilateur.
// =============================================================================
#include "NKImage/Core/NkImage.h"
#include "NKTime/NkChrono.h"

#include <cstdio>
#include <utility>

using namespace nkentseu;

namespace {
    constexpr int COTE_SOURCE = 2048;
    constexpr int COTE_RESULTAT = 256;
    constexpr int REPETITIONS = 20;

    // Un des quatre filtres, avec ce qu'on en a mesuré.
    struct Essai {
        const char *nom;      // écrit tel quel dans mesures.txt
        const char *fichier;  // PNG du dernier résultat
        NkResizeFilter filtre;
        double moyenneUs = 0.0;
        long long differences = 0;
        NkImage resultat;
    };

    // Compte les pixels qui diffèrent sur au moins un des quatre canaux.
    long long CompterDifferences(const NkImage &a, const NkImage &b) {
        long long compte = 0;
        for (int y = 0; y < a.Height(); ++y) {
            for (int x = 0; x < a.Width(); ++x) {
                const auto pa = a.GetPixel(x, y);
                const auto pb = b.GetPixel(x, y);
                if (pa.r != pb.r || pa.g != pb.g || pa.b != pb.b || pa.a != pb.a) {
                    ++compte;
                }
            }
        }
        return compte;
    }
}

int main(int argc, char **argv) {
    const char *chemin = (argc > 1) ? argv[1] : "photo.jpg";

    // ── 1) La source : la photo, réduite en 2048 x 2048 ───────────────────────
    NkImage photo;
    if (!photo.Load(chemin)) {
        std::printf("[ERREUR] impossible de charger '%s'.\n", chemin);
        std::printf("Le chemin est relatif au dossier d'ou le programme est lance.\n");
        return 1;
    }
    const NkImage source = photo.Resize(COTE_SOURCE, COTE_SOURCE, NkResizeFilter::NK_BILINEAR);
    std::printf("Photo : %s, %d x %d. Source : %d x %d.\n", chemin, photo.Width(), photo.Height(),
                source.Width(), source.Height());

    Essai essais[] = {
        {"NK_NEAREST", "nearest.png", NkResizeFilter::NK_NEAREST},
        {"NK_BILINEAR", "bilinear.png", NkResizeFilter::NK_BILINEAR},
        {"NK_BICUBIC", "bicubic.png", NkResizeFilter::NK_BICUBIC},
        {"NK_LANCZOS3", "lanczos3.png", NkResizeFilter::NK_LANCZOS3},
    };
    constexpr int NB = 4;

    // Une réduction à blanc, non chronométrée : la toute première paie des choses
    // que les suivantes ne paient pas (pages mémoire, caches du processeur) et
    // fausserait la moyenne du premier filtre mesuré.
    NkImage chauffe = source.Resize(COTE_RESULTAT, COTE_RESULTAT, NkResizeFilter::NK_BILINEAR);
    (void)chauffe;

    // ── 2) et 3) Réduire et chronométrer ──────────────────────────────────────
    for (int i = 0; i < NB; ++i) {
        double totalUs = 0.0;
        for (int r = 0; r < REPETITIONS; ++r) {
            NkChrono chrono;
            NkImage reduite = source.Resize(COTE_RESULTAT, COTE_RESULTAT, essais[i].filtre);
            totalUs += chrono.Elapsed().ToMicroseconds();
            // On garde la dernière réduction : le résultat est le même à chaque tour.
            // NkImage ne se copie pas par affectation : on la DÉPLACE.
            if (r == REPETITIONS - 1) {
                essais[i].resultat = std::move(reduite);
            }
        }
        essais[i].moyenneUs = totalUs / REPETITIONS;
    }

    // ── 4) Enregistrer les quatre résultats ───────────────────────────────────
    for (int i = 0; i < NB; ++i) {
        if (!essais[i].resultat.SavePNG(essais[i].fichier)) {
            std::printf("[ERREUR] ecriture de %s impossible.\n", essais[i].fichier);
            return 2;
        }
    }

    // ── 5) Comparer chaque résultat au bilinéaire, pixel par pixel ────────────
    const NkImage &reference = essais[1].resultat;  // NK_BILINEAR
    for (int i = 0; i < NB; ++i) {
        essais[i].differences = CompterDifferences(essais[i].resultat, reference);
    }

    // ── 6) mesures.txt ────────────────────────────────────────────────────────
    std::FILE *f = std::fopen("mesures.txt", "w");
    if (!f) {
        std::printf("[ERREUR] mesures.txt impossible a ecrire.\n");
        return 3;
    }
    for (int i = 0; i < NB; ++i) {
        std::fprintf(f, "%s : %.0f us, differences : %lld\n", essais[i].nom, essais[i].moyenneUs,
                     essais[i].differences);
        std::printf("%s : %.0f us, differences : %lld\n", essais[i].nom, essais[i].moyenneUs,
                    essais[i].differences);
    }
    std::fclose(f);
    std::printf("\nmesures.txt ecrit, ainsi que les quatre PNG.\n");
    return 0;
}
