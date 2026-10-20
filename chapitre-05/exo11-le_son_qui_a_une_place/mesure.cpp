// =============================================================================
// Chapitre 5, exercice 11 — mesurer ce que le moteur envoie à chaque oreille.
//
// Ce programme n'a ni fenêtre ni sortie sonore : le moteur de son tourne sur le
// dorsal NULL_OUTPUT, et le mélange est rendu à la main avec RenderToBuffer.
// L'auditeur est à l'origine et regarde vers -Z. Une pluie (bruit rose) est
// placée à deux mètres : à gauche, devant, puis à droite, avec positional vrai
// puis faux. Pour chaque cas, on rend 48 000 trames par blocs de 256 et on
// calcule la racine de la moyenne des carrés (RMS) de chaque oreille.
// Les six lignes sont écrites dans mesures.txt, sous la forme « devant 1 : G x D y ».
// =============================================================================
#include <cmath>
#include <cstdio>

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/NKMain.h"
#include "NKAudio/NkAudio.h"

using namespace nkentseu;
using namespace nkentseu::audio;

static void ConfigureAppData(NkAppData &d) {
    d.appName = "Ch05Exo11Mesure";
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

constexpr int FREQUENCE = 48000;
constexpr int TAMPON = 256;
constexpr int TRAMES = 48000;   // une seconde de mélange par mesure

struct Position {
    const char *nom;
    float x, y, z;
};

// Rend TRAMES trames par blocs de TAMPON et rend la RMS de l'oreille gauche et de la droite.
static void Mesurer(float &rmsGauche, float &rmsDroite) {
    AudioEngine &moteur = AudioEngine::Instance();
    float bloc[TAMPON * 2];  // gauche, droite, gauche, droite...
    double sommeG = 0.0;
    double sommeD = 0.0;
    int rendues = 0;
    while (rendues < TRAMES) {
        int n = TRAMES - rendues;
        if (n > TAMPON) n = TAMPON;
        moteur.RenderToBuffer(bloc, n, 2);
        for (int i = 0; i < n; ++i) {
            sommeG += (double)bloc[2 * i] * (double)bloc[2 * i];
            sommeD += (double)bloc[2 * i + 1] * (double)bloc[2 * i + 1];
        }
        rendues += n;
    }
    rmsGauche = (float)std::sqrt(sommeG / TRAMES);
    rmsDroite = (float)std::sqrt(sommeD / TRAMES);
}

int nkmain(const NkEntryState &state) {
    (void)state;
    AudioEngineConfig cfg;
    cfg.backend = AudioBackendType::NULL_OUTPUT;   // aucune sortie sonore
    cfg.sampleRate = FREQUENCE;
    cfg.bufferSize = TAMPON;
    AudioEngine &moteur = AudioEngine::Instance();
    if (!moteur.Initialize(cfg)) {
        std::printf("Moteur de son indisponible\n");
        return 1;
    }

    // L'auditeur : à l'origine, regard vers -Z, le haut vers +Y.
    moteur.SetListenerPosition(0.f, 0.f, 0.f);
    moteur.SetListenerOrientation(0.f, 0.f, -1.f, 0.f, 1.f, 0.f);

    AudioSample pluie = AudioGenerator::GenerateNoise(4.f, WaveformType::NOISE_PINK, FREQUENCE);

    const Position positions[3] = {
        {"gauche", -2.f, 0.f, 0.f},
        {"devant", 0.f, 0.f, -2.f},
        {"droite", 2.f, 0.f, 0.f},
    };

    std::FILE *fichier = std::fopen("mesures.txt", "w");
    for (const Position &pos : positions) {
        for (int positional = 1; positional >= 0; --positional) {
            VoiceParams p;
            p.looping = true;
            p.source3d.position[0] = pos.x;
            p.source3d.position[1] = pos.y;
            p.source3d.position[2] = pos.z;
            p.source3d.positional = (positional == 1);
            const AudioHandle voix = moteur.Play(pluie, p);

            float g = 0.f;
            float d = 0.f;
            Mesurer(g, d);
            moteur.Stop(voix);   // la source ne bouge pas pendant une mesure : on l'arrête avant la suivante

            std::printf("%s %d : G %.4f D %.4f\n", pos.nom, positional, (double)g, (double)d);
            if (fichier) std::fprintf(fichier, "%s %d : G %.4f D %.4f\n", pos.nom, positional, (double)g, (double)d);
        }
    }
    if (fichier) std::fclose(fichier);

    moteur.Shutdown();
    AudioLoader::Free(pluie);
    return 0;
}
