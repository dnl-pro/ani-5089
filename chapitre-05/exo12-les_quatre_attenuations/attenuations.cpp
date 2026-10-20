// =============================================================================
// Chapitre 5, exercice 12 — les quatre atténuations.
//
// Ce programme n'a ni fenêtre ni sortie sonore : le moteur tourne sur le dorsal
// NULL_OUTPUT et le mélange est rendu à la main avec RenderToBuffer.
// L'auditeur est à l'origine, il regarde vers -Z. Un sinus de 440 Hz joue en
// boucle, placé devant lui en (0, 0, -d). On mesure, pour chacun des quatre
// modèles d'atténuation (NONE, INVERSE, LINEAR, EXPONENTIAL), à cinq distances
// (0,5 ; 1 ; 2 ; 4 ; 8 m), la racine de la moyenne des carrés (RMS) des DEUX
// oreilles ensemble, sur 48 000 trames rendues par blocs de 256.
// Le gain est la mesure divisée par celle du modèle NONE à la même distance.
// Les quarante gains (rolloffFactor = 1, puis 2) sont écrits dans mesures.txt.
// =============================================================================
#include <cmath>
#include <cstdio>

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/NKMain.h"
#include "NKAudio/NkAudio.h"

using namespace nkentseu;
using namespace nkentseu::audio;

static void ConfigureAppData(NkAppData &d) {
    d.appName = "Ch05Exo12";
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

constexpr int FREQUENCE = 48000;
constexpr int TAMPON = 256;
constexpr int TRAMES = 48000;

// Rend TRAMES trames par blocs de TAMPON, et rend la RMS des deux oreilles ensemble.
static float RmsDeuxOreilles() {
    AudioEngine &moteur = AudioEngine::Instance();
    float bloc[TAMPON * 2];  // gauche, droite, gauche, droite...
    double somme = 0.0;
    int rendues = 0;
    while (rendues < TRAMES) {
        int n = TRAMES - rendues;
        if (n > TAMPON) n = TAMPON;
        moteur.RenderToBuffer(bloc, n, 2);
        for (int i = 0; i < 2 * n; ++i) {
            somme += (double)bloc[i] * (double)bloc[i];
        }
        rendues += n;
    }
    return (float)std::sqrt(somme / (double)(2 * TRAMES));
}

// Une mesure : le modèle, la distance et le rolloff donnés ; la source ne bouge pas pendant la mesure.
static float Mesurer(const AudioSample &sinus, AttenuationModel modele, float distance, float rolloff) {
    AudioEngine &moteur = AudioEngine::Instance();
    VoiceParams p;
    p.looping = true;
    p.source3d.positional = true;
    p.source3d.minDistance = 1.0f;
    p.source3d.maxDistance = 100.0f;
    p.source3d.rolloffFactor = rolloff;
    p.source3d.attenuationModel = modele;
    p.source3d.position[0] = 0.f;
    p.source3d.position[1] = 0.f;
    p.source3d.position[2] = -distance;
    const AudioHandle voix = moteur.Play(sinus, p);
    const float rms = RmsDeuxOreilles();
    moteur.Stop(voix);   // on arrête la voix avant de placer la suivante
    return rms;
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
    moteur.SetListenerPosition(0.f, 0.f, 0.f);
    moteur.SetListenerOrientation(0.f, 0.f, -1.f, 0.f, 1.f, 0.f);

    // Un sinus doux : amplitude 0,1, pour que le moteur ne l'adoucisse pas à la sortie.
    AudioSample sinus = AudioGenerator::GenerateTone(440.f, 2.f, WaveformType::SINE, FREQUENCE, 0.1f);

    const AttenuationModel modeles[4] = {AttenuationModel::NONE, AttenuationModel::INVERSE,
                                         AttenuationModel::LINEAR, AttenuationModel::EXPONENTIAL};
    const char *noms[4] = {"NONE", "INVERSE", "LINEAR", "EXPONENTIAL"};
    const float distances[5] = {0.5f, 1.f, 2.f, 4.f, 8.f};
    const float rolloffs[2] = {1.f, 2.f};

    std::FILE *fichier = std::fopen("mesures.txt", "w");
    for (const float rolloff : rolloffs) {
        // Les mesures du modèle NONE servent de référence à chaque distance.
        float reference[5];
        for (int j = 0; j < 5; ++j) {
            reference[j] = Mesurer(sinus, AttenuationModel::NONE, distances[j], rolloff);
        }
        for (int m = 0; m < 4; ++m) {
            for (int j = 0; j < 5; ++j) {
                const float rms = Mesurer(sinus, modeles[m], distances[j], rolloff);
                const float gain = reference[j] > 0.f ? rms / reference[j] : 0.f;
                std::printf("%s, rolloff %g, %g m : %.3f\n", noms[m], (double)rolloff, (double)distances[j], (double)gain);
                if (fichier) {
                    std::fprintf(fichier, "%s, rolloff %g, %g m : %.3f\n", noms[m], (double)rolloff,
                                 (double)distances[j], (double)gain);
                }
            }
        }
    }
    if (fichier) std::fclose(fichier);

    moteur.Shutdown();
    AudioLoader::Free(sinus);
    return 0;
}
