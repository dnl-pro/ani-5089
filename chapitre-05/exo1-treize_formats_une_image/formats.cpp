// =============================================================================
// Chapitre 5, exercice 1 — treize formats, une image.
//
// Le programme :
//   1. charge une photo avec NkImage::Load ;
//   2. la fait écrire par le moteur dans cinq formats : png, jpeg, bmp, qoi, gif ;
//   3. RECHARGE chacun des cinq fichiers dans un nouveau NkImage ;
//   4. écrit formats.txt, une ligne par format :  format : N octets, L x H
//      (ou  format : N octets, illisible  si le fichier ne se relit pas) ;
//   5. compare les pixels relus à ceux de la photo, pour mesurer ce que chaque
//      format perd, et écrit un brouillon de choix.txt à partir de ces mesures.
//
// Usage :  formats.exe [photo]      (par défaut : photo.jpg)
//
// Le chemin de la photo est relatif au dossier D'OÙ LE PROGRAMME EST LANCÉ, pas
// à l'endroit où se trouve l'exécutable. Lancez-le depuis le dossier du projet.
// =============================================================================
#include "NKImage/Core/NkImage.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace nkentseu;

namespace {
    enum class Codec { PNG, JPEG, BMP, QOI, GIF };

    // Un des cinq formats essayés, avec tout ce qu'on en a mesuré.
    struct Essai {
        const char *nom;      // écrit tel quel dans formats.txt
        const char *fichier;  // fichier écrit par le moteur, puis relu
        Codec codec;
        bool ecrit = false;
        long long octets = 0;  // taille sur le disque, lue APRÈS l'écriture
        bool relu = false;
        int largeur = 0;
        int hauteur = 0;
        bool memesDimensions = false;
        bool compare = false;      // les pixels ont pu être comparés à ceux de la photo
        double ecartMoyen = 0.0;   // niveaux perdus par canal couleur, sur 255
        int ecartMax = 0;
    };

    std::string Formater(const char *format, ...) {
        char tampon[1024];
        va_list args;
        va_start(args, format);
        std::vsnprintf(tampon, sizeof(tampon), format, args);
        va_end(args);
        return std::string(tampon);
    }

    // Taille d'un fichier sur le disque, en octets ; -1 si on ne peut pas l'ouvrir.
    // On la lit sur le fichier lui-même : le nom du fichier ne dit rien de sa taille.
    long long TailleSurDisque(const char *chemin) {
        std::FILE *fichier = std::fopen(chemin, "rb");
        if (fichier == nullptr) {
            return -1;
        }
        long long taille = -1;
        if (std::fseek(fichier, 0, SEEK_END) == 0) {
            taille = (long long)std::ftell(fichier);
        }
        std::fclose(fichier);
        return taille;
    }

    bool Ecrire(const NkImage &photo, const Essai &essai) {
        switch (essai.codec) {
        case Codec::PNG:
            return photo.SavePNG(essai.fichier);
        case Codec::JPEG:
            return photo.SaveJPEG(essai.fichier, 90);
        case Codec::BMP:
            return photo.SaveBMP(essai.fichier);
        case Codec::QOI:
            return photo.SaveQOI(essai.fichier);
        case Codec::GIF:
            return photo.SaveGIF(essai.fichier);
        }
        return false;
    }

    // Ramène une image en RGBA 8 bits pour comparer deux images de formats pixel différents.
    NkImage EnRGBA(const NkImage &image) {
        if (image.Format() == NkImagePixelFormat::NK_RGBA32) {
            return image.Copy();
        }
        return image.Convert(NkImagePixelFormat::NK_RGBA32);
    }

    // Écart entre les pixels de départ et les pixels relus : moyen et maximal,
    // par canal couleur (rouge, vert, bleu ; l'alpha est ignoré).
    bool ComparerPixels(const NkImage &depart, const NkImage &relue, double &ecartMoyen, int &ecartMax) {
        if (depart.IsHDR() || relue.IsHDR()) {
            return false;
        }
        if (depart.Width() != relue.Width() || depart.Height() != relue.Height()) {
            return false;
        }
        const NkImage a = EnRGBA(depart);
        const NkImage b = EnRGBA(relue);
        if (!a.IsValid() || !b.IsValid()) {
            return false;
        }

        unsigned long long somme = 0;
        int maxi = 0;
        for (int y = 0; y < a.Height(); ++y) {
            const unsigned char *ligneA = a.RowPtr(y);
            const unsigned char *ligneB = b.RowPtr(y);
            for (int x = 0; x < a.Width(); ++x) {
                for (int c = 0; c < 3; ++c) {
                    const int pa = ligneA[x * 4 + c];
                    const int pb = ligneB[x * 4 + c];
                    const int ecart = pa > pb ? pa - pb : pb - pa;
                    somme += (unsigned long long)ecart;
                    if (ecart > maxi) {
                        maxi = ecart;
                    }
                }
            }
        }
        const double echantillons = (double)a.Width() * (double)a.Height() * 3.0;
        ecartMoyen = (double)somme / echantillons;
        ecartMax = maxi;
        return true;
    }

    bool Sauver(const char *chemin, const std::string &texte) {
        std::FILE *fichier = std::fopen(chemin, "wb");
        if (fichier == nullptr) {
            return false;
        }
        const size_t ecrits = std::fwrite(texte.data(), 1, texte.size(), fichier);
        std::fclose(fichier);
        return ecrits == texte.size();
    }

    bool Existe(const char *chemin) {
        std::FILE *fichier = std::fopen(chemin, "rb");
        if (fichier == nullptr) {
            return false;
        }
        std::fclose(fichier);
        return true;
    }

    // Brouillon de choix.txt : chaque chiffre vient des mesures de CETTE exécution.
    // Les phrases sont à relire : le choix et la surprise sont les vôtres.
    std::string Brouillon(const std::vector<Essai> &essais, const NkImage &photo, long long octetsMemoire) {
        // Meilleur format pour un mur : le plus petit des formats SANS perte mesurée
        // (relu, mêmes dimensions, écart maximal nul).
        const Essai *meilleur = nullptr;
        for (const Essai &e : essais) {
            const bool sansPerte = e.relu && e.memesDimensions && e.compare && e.ecartMax == 0;
            if (sansPerte && (meilleur == nullptr || e.octets < meilleur->octets)) {
                meilleur = &e;
            }
        }
        const Essai *jpeg = nullptr;
        const Essai *gif = nullptr;
        const Essai *bmp = nullptr;
        const Essai *plusPetit = nullptr;
        const Essai *plusGros = nullptr;
        for (const Essai &e : essais) {
            if (e.codec == Codec::JPEG) {
                jpeg = &e;
            }
            if (e.codec == Codec::GIF) {
                gif = &e;
            }
            if (e.codec == Codec::BMP) {
                bmp = &e;
            }
            if (!e.relu) {
                continue;
            }
            if (plusPetit == nullptr || e.octets < plusPetit->octets) {
                plusPetit = &e;
            }
            if (plusGros == nullptr || e.octets > plusGros->octets) {
                plusGros = &e;
            }
        }

        std::string t;

        // ── Mon format pour un mur ───────────────────────────────────────────────
        t += "Mon format pour un mur : ";
        if (meilleur != nullptr) {
            t += std::string(meilleur->nom) + ".\n\n";
        } else {
            t += "aucun format sans perte ne s'est relu à l'identique, je ne peux pas en désigner un.\n\n";
        }

        // ── Pourquoi ─────────────────────────────────────────────────────────────
        t += "Pourquoi : ";
        t += Formater("sur ma photo de %d x %d pixels, j'ai mesuré sur le disque : ", photo.Width(), photo.Height());
        bool premier = true;
        for (const Essai &e : essais) {
            if (!e.relu) {
                continue;
            }
            t += Formater("%s%s %lld octets", premier ? "" : ", ", e.nom, e.octets);
            premier = false;
        }
        t += ". ";
        if (meilleur != nullptr) {
            t += Formater("Parmi les formats sans perte, le %s est le plus petit (%lld octets) et il se relit à "
                          "l'identique : écart maximal de %d niveau sur les pixels. ",
                          meilleur->nom, meilleur->octets, meilleur->ecartMax);
            if (bmp != nullptr && bmp->relu && bmp != meilleur && meilleur->octets > 0) {
                t += Formater("Le bmp, qui ne compresse pas, coûte %.1f fois plus de place. ",
                              (double)bmp->octets / (double)meilleur->octets);
            }
        }
        if (jpeg != nullptr && jpeg->relu && jpeg->compare) {
            t += Formater("Le jpeg est compressé avec perte : relu, il s'écarte des pixels de départ de %.2f niveaux "
                          "par canal en moyenne (sur 255), et jusqu'à %d au pire ; ces pertes sont définitives et "
                          "s'ajoutent à chaque réécriture. ",
                          jpeg->ecartMoyen, jpeg->ecartMax);
        }
        if (gif != nullptr && gif->relu && gif->compare) {
            t += Formater("Le gif, limité à 256 couleurs, s'écarte de %.2f niveaux en moyenne et de %d au pire. ",
                          gif->ecartMoyen, gif->ecartMax);
        }
        t += "Une texture de mur se regarde de près et se relit souvent : je ne veux pas y ajouter d'erreurs "
             "de compression.\n\n";

        // ── Ce qui m'a surpris ───────────────────────────────────────────────────
        t += "Ce qui m'a surpris : ";
        t += Formater("une fois chargée, l'image coûte %lld octets en mémoire (%d x %d pixels, %d canaux), quel que "
                      "soit le format du fichier : le fichier le plus léger ne réduit pas ce coût. ",
                      octetsMemoire, photo.Width(), photo.Height(), photo.Channels());
        if (plusPetit != nullptr && plusGros != nullptr && plusPetit->octets > 0 && plusPetit != plusGros) {
            t += Formater("Entre le plus gros fichier (%s, %lld octets) et le plus petit (%s, %lld octets), le "
                          "rapport est de %.1f. ",
                          plusGros->nom, plusGros->octets, plusPetit->nom, plusPetit->octets,
                          (double)plusGros->octets / (double)plusPetit->octets);
        }
        for (const Essai &e : essais) {
            if (!e.relu) {
                t += Formater("Le format %s, écrit par le moteur, ne s'est pas relu : je l'ai noté tel quel. ", e.nom);
            } else if (!e.memesDimensions) {
                t += Formater("Le %s s'est relu en %d x %d au lieu de %d x %d. ", e.nom, e.largeur, e.hauteur,
                              photo.Width(), photo.Height());
            }
        }
        t += "\n";
        return t;
    }
}  // namespace

int main(int argc, char **argv) {
    const char *cheminPhoto = (argc > 1) ? argv[1] : "photo.jpg";

    // ── 1) Charger la photo ───────────────────────────────────────────────────
    NkImage photo;
    if (!photo.Load(cheminPhoto)) {
        std::fprintf(stderr,
                     "[ERREUR] impossible de charger '%s'.\n"
                     "Le chemin est relatif au dossier d'ou le programme est lance : lancez-le depuis le dossier "
                     "qui contient la photo, ou donnez son chemin en argument.\n",
                     cheminPhoto);
        return 1;
    }
    const long long octetsMemoire = (long long)photo.TotalBytes();
    std::printf("Photo : %s, %d x %d pixels, %d canaux.\n", cheminPhoto, photo.Width(), photo.Height(),
                photo.Channels());
    std::printf("En memoire, chargee : %lld octets (ses pixels bruts, quel que soit son format).\n\n", octetsMemoire);

    // ── 2) Écrire cinq formats, puis 3) relire chacun ─────────────────────────
    std::vector<Essai> essais = {
        {"png", "sortie.png", Codec::PNG},   {"jpeg", "sortie.jpg", Codec::JPEG}, {"bmp", "sortie.bmp", Codec::BMP},
        {"qoi", "sortie.qoi", Codec::QOI},   {"gif", "sortie.gif", Codec::GIF},
    };

    for (Essai &essai : essais) {
        essai.ecrit = Ecrire(photo, essai);
        const long long taille = TailleSurDisque(essai.fichier);
        essai.octets = taille > 0 ? taille : 0;

        // Un fichier écrit n'est pas un fichier relisible : c'est la relecture qui compte.
        NkImage relue;
        essai.relu = relue.Load(essai.fichier);
        if (essai.relu) {
            essai.largeur = relue.Width();
            essai.hauteur = relue.Height();
            essai.memesDimensions = (essai.largeur == photo.Width() && essai.hauteur == photo.Height());
            essai.compare = ComparerPixels(photo, relue, essai.ecartMoyen, essai.ecartMax);
        }

        std::printf("%-5s ecriture %s, %lld octets sur le disque, ", essai.nom, essai.ecrit ? "ok " : "ECHEC", essai.octets);
        if (essai.relu) {
            std::printf("relu en %d x %d (%s la photo)", essai.largeur, essai.hauteur,
                        essai.memesDimensions ? "comme" : "DIFFERENT de");
            if (essai.compare) {
                std::printf(", ecart moyen %.2f / max %d", essai.ecartMoyen, essai.ecartMax);
            }
            std::printf("\n");
        } else {
            std::printf("ILLISIBLE (le moteur ne relit pas le fichier qu'il vient d'ecrire)\n");
        }
    }

    // ── 4) formats.txt : une ligne par format, venue de la relecture ──────────
    std::string lignes;
    for (const Essai &essai : essais) {
        if (essai.relu) {
            lignes += Formater("%s : %lld octets, %d x %d\n", essai.nom, essai.octets, essai.largeur, essai.hauteur);
        } else {
            lignes += Formater("%s : %lld octets, illisible\n", essai.nom, essai.octets);
        }
    }
    if (!Sauver("formats.txt", lignes)) {
        std::fprintf(stderr, "[ERREUR] formats.txt n'a pas pu etre ecrit.\n");
        return 2;
    }
    std::printf("\nformats.txt ecrit :\n%s", lignes.c_str());

    // ── 5) Brouillon de choix.txt ─────────────────────────────────────────────
    // On n'écrase jamais un choix.txt déjà relu et corrigé à la main.
    const char *cheminChoix = Existe("choix.txt") ? "choix_brouillon.txt" : "choix.txt";
    if (Sauver(cheminChoix, Brouillon(essais, photo, octetsMemoire))) {
        std::printf("\n%s ecrit a partir de ces mesures : relisez-le et corrigez ce qui n'est pas votre avis.\n",
                    cheminChoix);
    }
    return 0;
}
