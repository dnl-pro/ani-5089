// =============================================================================
//  Exo 10 : deux chaines d'echange, une par oeil, et un redimensionnement.
//
//  Chaque oeil a SA chaine : son anneau, son drapeau d'emprunt, ses poignees.
//  A chaque image, pour chaque oeil, les quatre temps dans l'ordre :
//      1. emprunter une image        2. dessiner dedans
//      3. la rendre                  4. soumettre la derniere rendue
//
//  Quand la fenetre change de taille, redimensionner les cibles ne suffit pas :
//  les poignees gardees par les chaines ne pointent plus vers les bonnes
//  images. On detruit donc les deux chaines, on les cree a nouveau, et on
//  enregistre a nouveau leurs images.
//
//  Le programme ouvre une vraie fenetre, sans carte graphique ni renderer : une
//  chaine d'echange est de la comptabilite. Il demande lui-meme un
//  redimensionnement a la dixieme image, et il detecte aussi celui qu'on fait a
//  la main en tirant sur un bord. Il ecrit trace.txt et resize.txt dans le
//  dossier donne par DEUX_DOSSIER (le dossier courant par defaut).
// =============================================================================
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkWindowEvent.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

using namespace nkentseu;

static void ConfigureAppData(NkAppData &d) { d.appName = "DeuxChaines"; }
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

namespace {

	const std::uint32_t kImages = 2; // images par chaine
	const int kImagesMinimum = 30;
	const int kImagesMaximum = 300;
	const int kImageDuRedimensionnement = 10;

	// Une chaine : un anneau de poignees opaques et un drapeau d'emprunt.
	struct Chaine {
		std::uint64_t poignees[4] = { 0, 0, 0, 0 };
		std::uint32_t nombre = 0;
		std::uint32_t suivant = 0;
		bool emprunte = false;
		bool vivante = false;

		void Creer(std::uint32_t n) {
			nombre = n;
			suivant = 0;
			emprunte = false;
			vivante = true;
			for (int i = 0; i < 4; ++i) {
				poignees[i] = 0; // une chaine neuve est pleine de zeros
			}
		}

		void Detruire() {
			vivante = false;
			nombre = 0;
			suivant = 0;
			emprunte = false;
			for (int i = 0; i < 4; ++i) {
				poignees[i] = 0;
			}
		}

		void Enregistrer(std::uint32_t index, std::uint64_t poignee) {
			if (index < nombre) {
				poignees[index] = poignee;
			}
		}

		// Temps 1 : refuse le double emprunt.
		bool Emprunter(std::uint32_t &index) {
			if (!vivante || emprunte) return false;
			emprunte = true;
			index = suivant;
			return true;
		}

		// Temps 3 : refuse de rendre sans avoir emprunte, puis avance.
		bool Rendre() {
			if (!vivante || !emprunte) return false;
			emprunte = false;
			suivant = (suivant + 1) % nombre;
			return true;
		}

		std::uint32_t DerniereRendue() const {
			return (suivant + nombre - 1) % nombre;
		}
	};

	// Les poignees d'une generation : une valeur par oeil et par image, jamais
	// nulle. Une nouvelle generation donne de nouvelles valeurs.
	std::uint64_t FabriquerPoignee(std::uint64_t generation, std::uint32_t oeil,
								   std::uint32_t index) {
		return (generation << 32) | ((std::uint64_t)(oeil + 1) << 8) | (index + 1);
	}

	void EnregistrerLesImages(Chaine &chaine, std::uint64_t generation,
							  std::uint32_t oeil) {
		for (std::uint32_t i = 0; i < chaine.nombre; ++i) {
			chaine.Enregistrer(i, FabriquerPoignee(generation, oeil, i));
		}
	}

	// Les quatre temps pour un oeil. Renvoie l'index emprunte, ou -1 si un
	// temps a ete refuse ou si la poignee de l'image est nulle.
	int UneImage(Chaine &chaine) {
		std::uint32_t index = 0;
		if (!chaine.Emprunter(index)) return -1;			   // 1
		const bool poigneeValide = chaine.poignees[index] != 0; // 2 : on dessine
		if (!chaine.Rendre()) return -1;						// 3
		const std::uint32_t soumise = chaine.DerniereRendue();	// 4 : on soumet
		if (!poigneeValide || soumise != index) return -1;
		return (int)index;
	}

	std::string Dossier() {
		const char *d = getenv("DEUX_DOSSIER");
		return (d != nullptr && *d != '\0') ? std::string(d) : std::string(".");
	}

} // namespace

int nkmain(const NkEntryState &state) {
	(void)state;

	NkWindowConfig cfg;
	cfg.title = "Deux chaines";
	cfg.width = 1280;
	cfg.height = 720;
	cfg.centered = true;

	NkWindow fenetre(cfg);
	if (!fenetre.IsValid()) {
		std::fprintf(stderr, "la fenetre n'a pas pu s'ouvrir\n");
		return 1;
	}

	const std::string dossier = Dossier();
	std::FILE *trace = std::fopen((dossier + "/trace.txt").c_str(), "w");
	std::FILE *resize = std::fopen((dossier + "/resize.txt").c_str(), "w");
	if (trace == nullptr || resize == nullptr) {
		std::fprintf(stderr, "impossible d'ecrire trace.txt / resize.txt\n");
		return 1;
	}

	// Deux chaines vraiment separees : une par oeil.
	Chaine chaines[2];
	std::uint64_t generation = 1;
	for (std::uint32_t oeil = 0; oeil < 2; ++oeil) {
		chaines[oeil].Creer(kImages);
		EnregistrerLesImages(chaines[oeil], generation, oeil);
	}

	auto taille = fenetre.GetSize();
	bool redimensionnementFait = false;
	bool tailleDemandee = false;
	int erreurs = 0;
	int image = 0;
	bool tourne = true;

	while (tourne && fenetre.IsOpen() && image < kImagesMaximum) {
		NkEvent *ev = nullptr;
		while (NkEvents().PollEvent(ev)) {
			if (ev->As<NkWindowCloseEvent>() != nullptr) {
				tourne = false;
			}
		}
		if (!tourne) break;

		// Le redimensionnement, demande par le programme a l'image 10.
		if (image == kImageDuRedimensionnement && !tailleDemandee) {
			fenetre.SetSize(960, 540);
			tailleDemandee = true;
		}

		// La taille de la fenetre a change : on reconstruit les chaines.
		const auto maintenant = fenetre.GetSize();
		if (maintenant.x != taille.x || maintenant.y != taille.y) {
			std::fprintf(resize, "redimensionne la fenetre de %ux%u a %ux%u\n",
						 (unsigned)taille.x, (unsigned)taille.y,
						 (unsigned)maintenant.x, (unsigned)maintenant.y);
			taille = maintenant;

			chaines[0].Detruire();
			std::fprintf(resize, "detruit la chaine gauche\n");
			chaines[1].Detruire();
			std::fprintf(resize, "detruit la chaine droite\n");

			++generation;
			chaines[0].Creer(kImages);
			std::fprintf(resize, "recree la chaine gauche (%u images)\n", (unsigned)kImages);
			chaines[1].Creer(kImages);
			std::fprintf(resize, "recree la chaine droite (%u images)\n", (unsigned)kImages);

			EnregistrerLesImages(chaines[0], generation, 0);
			std::fprintf(resize, "reenregistre les images de la chaine gauche\n");
			EnregistrerLesImages(chaines[1], generation, 1);
			std::fprintf(resize, "reenregistre les images de la chaine droite\n");
			std::fflush(resize);
			redimensionnementFait = true;
		}

		// Une image : les quatre temps pour chaque oeil.
		const int gauche = UneImage(chaines[0]);
		const int droit = UneImage(chaines[1]);
		if (gauche < 0 || droit < 0) {
			++erreurs;
		}
		++image;
		std::fprintf(trace, "image : %d, gauche : %d, droit : %d\n", image, gauche, droit);
		std::fflush(trace);

		if (image >= kImagesMinimum && redimensionnementFait) {
			tourne = false;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}

	std::fclose(trace);
	std::fclose(resize);
	std::printf("%d images, redimensionnement %s, %d erreur(s)\n", image,
				redimensionnementFait ? "fait" : "NON FAIT", erreurs);
	return (erreurs == 0 && redimensionnementFait) ? 0 : 1;
}
