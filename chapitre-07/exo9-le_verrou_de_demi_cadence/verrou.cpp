// =============================================================================
//  Chapitre 7, exercice 9 — le verrou de demi-cadence.
//
//  La salle ci-dessous (étape 5 du kit), rendue plus lourde : MASALLE_COPIES=K la soumet K fois,
//  chaque copie décalée de 5 mètres le long de x. K = 1 est la salle telle qu'elle est.
//  MASALLE_HZ est le taux de rafraîchissement de l'écran, lu dans les paramètres d'affichage
//  Windows ; sa période est 1000 / MASALLE_HZ ms.
//  Le verrou (MASALLE_VERROU=1) : à la fin de CHAQUE image, attendre que deux périodes d'écran
//  se soient écoulées depuis le DÉBUT de cette image. Sans lui, l'image repart aussitôt.
//  L'attente doit être précise, et NkChrono::SleepMilliseconds ne l'est pas : sans
//  BeginPreciseTiming(), un sommeil de 1 à 12 ms dure ~15,5 ms quoi qu'on lui demande, et avec
//  la minuterie fine un sommeil de 1 ms dure ~1,9 ms (commentaire de NKTime/NkChrono.h). Donc on
//  demande la minuterie fine (dans les deux mesures, pour qu'elles se comparent), on dort
//  jusqu'à environ 2 ms avant la cible, puis on tourne sur l'horloge jusqu'à l'instant exact.
//  Mesure : mille intervalles entre deux débuts d'image successifs (les trente premières
//  images laissées de côté), puis la moyenne, l'écart-type et le maximum, en millisecondes.
//  L'intervalle va d'un début d'image au suivant, ce n'est pas la durée du travail : le verrou
//  attend après le travail.
//
//  (En-tête d'origine, étape 5 — la salle, en cubes, aux dimensions du chapitre 2.)
//
//  Toutes les dimensions sont en MÈTRES, et viennent du tableau du chapitre 7 :
//      sol 4 x 4, murs 2,5 de haut, porte 0,9 x 2,0,
//      deux fenêtres 1,2 x 1,0, table 1,2 x 0,8 avec le plateau à 0,75.
//
//  Le cube et la sphère de NkMeshSystem sont UNITAIRES, de demi-étendue 0,5 :
//  une échelle S donne donc une taille S, et poser un objet au sol demande de
//  le translater de S/2. Le plan de GetPlane() est unitaire LUI AUSSI, de
//  demi-étendue 0,5 : une échelle S y donne un côté S, et non un demi-côté.
// =============================================================================
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkWindowEvent.h"

#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Materials/NkMaterialCollection.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"

#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKTime/NkChrono.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace nkentseu;
using namespace nkentseu::renderer;

static void ConfigureAppData(NkAppData &d) { d.appName = "Ch07Exo9"; }
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

namespace {

	// ── Les dimensions de la salle, en un seul endroit ────────────────────
	// Elles sont ici, nommées, et pas semées dans le code : au chapitre 21 on
	// les lira depuis un fichier, et ce sont ces noms-là qui deviendront des
	// champs.
	const float32 kPiece = 4.00f;	  // côté intérieur du sol
	const float32 kPlafond = 2.50f;	  // hauteur sous plafond
	const float32 kMur = 0.10f;		  // épaisseur d'un mur
	const float32 kPorteL = 0.90f;	  // porte : largeur
	const float32 kPorteH = 2.00f;	  //         hauteur
	const float32 kFenL = 1.20f;	  // fenêtre : largeur
	const float32 kFenH = 1.00f;	  //           hauteur
	const float32 kFenY = 1.40f;	  //           hauteur de son centre
	const float32 kTableL = 1.20f;	  // table : longueur
	const float32 kTableP = 0.80f;	  //         profondeur
	const float32 kTableH = 0.75f;	  //         hauteur du plateau
	const float32 kYeux = 1.70f;	  // hauteur des yeux, debout

	const float32 kDemi = kPiece * 0.5f;

	// Le décalage de la copie en cours de la salle, le long de x, en mètres (0 pour la première).
	float32 gDecalageX = 0.f;

	// Un pavé posé dans la salle. `centre` est le centre géométrique, `taille`
	// les trois dimensions en mètres : on ne manipule jamais autre chose.
	void Pave(NkRender3D *r3d, NkMeshSystem *meshes, const NkVec3f &centre,
			  const NkVec3f &taille, const NkVec3f &teinte, float32 rugosite = 1.f) {
		NkDrawCall3D dc;
		dc.mesh = meshes->GetCube();
		const NkVec3f c = { centre.x + gDecalageX, centre.y, centre.z };
		dc.transform = NkMat4f::Translate(c) * NkMat4f::Scale(taille);
		dc.aabb = { { c.x - taille.x * 0.5f, c.y - taille.y * 0.5f,
					  c.z - taille.z * 0.5f },
					{ c.x + taille.x * 0.5f, c.y + taille.y * 0.5f,
					  c.z + taille.z * 0.5f } };
		dc.tint = teinte;
		dc.metallic = 0.f;
		dc.roughness = rugosite;
		r3d->Submit(dc);
	}

	void SoumettreLaSalle(NkRender3D *r3d, NkMeshSystem *meshes) {
		const NkVec3f grisMur = { 0.72f, 0.70f, 0.66f };
		const NkVec3f grisSol = { 0.38f, 0.36f, 0.34f };
		const NkVec3f bois = { 0.55f, 0.38f, 0.22f };
		const NkVec3f vitre = { 0.75f, 0.85f, 0.92f };

		{ // le sol : le plan est unitaire, une échelle S donne un côté S
			NkDrawCall3D dc;
			dc.mesh = meshes->GetPlane();
			dc.transform = NkMat4f::Translate({ gDecalageX, 0.f, 0.f }) * NkMat4f::Scale({ kPiece, 1.f, kPiece });
			dc.aabb = { { -kDemi + gDecalageX, 0.f, -kDemi }, { kDemi + gDecalageX, 0.f, kDemi } };
			dc.castShadow = false;
			dc.tint = grisSol;
			dc.metallic = 0.f;
			dc.roughness = 1.f;
			r3d->Submit(dc);
		}

		const float32 yMur = kPlafond * 0.5f;
		const float32 bord = kDemi + kMur * 0.5f;

		// Quatre murs. La porte est dans celui du fond, les fenêtres à gauche.
		Pave(r3d, meshes, { 0.f, yMur, -bord }, { kPiece, kPlafond, kMur }, grisMur);
		Pave(r3d, meshes, { 0.f, yMur, bord }, { kPiece, kPlafond, kMur }, grisMur);
		Pave(r3d, meshes, { -bord, yMur, 0.f }, { kMur, kPlafond, kPiece }, grisMur);
		Pave(r3d, meshes, { bord, yMur, 0.f }, { kMur, kPlafond, kPiece }, grisMur);

		// Le plafond. On l'oublie toujours, et le trou ne se voit qu'en levant
		// les yeux, c'est-à-dire le jour de la démonstration.
		Pave(r3d, meshes, { 0.f, kPlafond + kMur * 0.5f, 0.f },
			 { kPiece, kMur, kPiece }, grisMur);

		// La porte, fermée jusqu'au chapitre 17 : un panneau plaqué contre le
		// mur du fond, décalé pour ne pas se battre avec lui en profondeur.
		Pave(r3d, meshes, { -0.70f, kPorteH * 0.5f, -kDemi + 0.02f },
			 { kPorteL, kPorteH, 0.04f }, bois, 0.6f);

		// Deux fenêtres sur le mur de gauche, pour qu'il y ait un dehors.
		Pave(r3d, meshes, { -kDemi + 0.02f, kFenY, -0.90f },
			 { 0.04f, kFenH, kFenL }, vitre, 0.15f);
		Pave(r3d, meshes, { -kDemi + 0.02f, kFenY, 0.90f },
			 { 0.04f, kFenH, kFenL }, vitre, 0.15f);

		// La table : un plateau et quatre pieds. Le chapitre 12 y posera un
		// objet, le 21 la déplacera.
		const float32 tx = 0.60f, tz = 0.70f;
		const float32 ep = 0.05f;
		Pave(r3d, meshes, { tx, kTableH - ep * 0.5f, tz },
			 { kTableL, ep, kTableP }, bois, 0.5f);

		const float32 pied = 0.06f;
		const float32 hp = kTableH - ep;
		for (int i = 0; i < 4; ++i) {
			const float32 dx = ((i & 1) ? 1.f : -1.f) * (kTableL * 0.5f - pied);
			const float32 dz = ((i & 2) ? 1.f : -1.f) * (kTableP * 0.5f - pied);
			Pave(r3d, meshes, { tx + dx, hp * 0.5f, tz + dz },
				 { pied, hp, pied }, bois, 0.6f);
		}

		// Le repère d'un mètre du chapitre 9 : un cube d'un mètre de côté,
		// posé au sol, contre lequel on juge l'échelle de tout le reste.
		Pave(r3d, meshes, { -1.10f, 0.50f, 1.20f }, { 1.f, 1.f, 1.f },
			 { 0.85f, 0.80f, 0.30f }, 0.8f);
	}

	uint64 EnvU64(const char *nom, uint64 defaut) {
		const char *t = getenv(nom);
		if (t == nullptr || *t == '\0') return defaut;
		return (uint64)strtoull(t, nullptr, 10);
	}

	double EnvDouble(const char *nom, double defaut) {
		const char *t = getenv(nom);
		if (t == nullptr || *t == '\0') return defaut;
		return strtod(t, nullptr);
	}

	// ── Le verrou : attendre un instant précis ────────────────────────────────
	// Dormir ne suffit pas : NkChrono::SleepMilliseconds dure ~15,5 ms quoi qu'on demande
	// (entre 1 et 12) sans BeginPreciseTiming(), et ~1,9 ms pour 1 ms avec elle. On dort donc
	// seulement la partie « grossière » de l'attente, en laissant 2 ms de marge, puis on tourne
	// sur l'horloge (qui ne revient jamais en arrière) jusqu'à l'instant exact.
	void AttendreJusqua(double cibleNs) {
		for (;;) {
			const double restantMs = (cibleNs - NkChrono::Now().nanoseconds) * 1e-6;
			if (restantMs <= 0.0) {
				return;
			}
			if (restantMs > 3.0) {
				NkChrono::SleepMilliseconds((int64)(restantMs - 2.0));
			}
			// sinon : on tourne, pour une précision d'une fraction de milliseconde
		}
	}

	// ── Les mesures : trente images laissées de côté, puis mille intervalles ──
	constexpr size_t IMAGES_IGNOREES = 30;
	constexpr size_t INTERVALLES = 1000;

	// Les intervalles entre deux débuts d'image successifs, en millisecondes. L'image numéro i
	// commence à debuts[i] ; l'intervalle numéro i vaut debuts[i + 1] - debuts[i].
	bool LesIntervalles(const std::vector<double> &debutsNs, std::vector<double> &intervallesMs) {
		if (debutsNs.size() < IMAGES_IGNOREES + INTERVALLES + 1) {
			return false;
		}
		intervallesMs.clear();
		for (size_t i = IMAGES_IGNOREES; i < IMAGES_IGNOREES + INTERVALLES; ++i) {
			intervallesMs.push_back((debutsNs[i + 1] - debutsNs[i]) * 1e-6);
		}
		return true;
	}

	// La moyenne, l'écart-type (racine de la moyenne des écarts carrés à la moyenne) et le maximum.
	void ImprimerLaMesure(const char *nom, const std::vector<double> &intervallesMs) {
		const size_t n = intervallesMs.size();
		double somme = 0.0, maximum = 0.0;
		for (const double d : intervallesMs) {
			somme += d;
			if (d > maximum) maximum = d;
		}
		const double moyenne = somme / (double)n;
		double sommeEcarts = 0.0;
		for (const double d : intervallesMs) {
			sommeEcarts += (d - moyenne) * (d - moyenne);
		}
		const double ecartType = std::sqrt(sommeEcarts / (double)n);
		std::printf("%s : moyenne %.3f ms, ecart-type %.3f ms, max %.3f ms\n", nom, moyenne, ecartType, maximum);
		std::printf("detail %s : %zu intervalles, %.2f images par seconde\n", nom, n, 1000.0 / moyenne);
	}

} // namespace

int nkmain(const NkEntryState &state) {
	(void)state;

	// Les réglages de l'expérience.
	const uint64 copies = EnvU64("MASALLE_COPIES", 1);
	const double hz = EnvDouble("MASALLE_HZ", 60.0);
	const double periodeMs = 1000.0 / (hz > 1.0 ? hz : 60.0);
	const bool verrou = (EnvU64("MASALLE_VERROU", 0) != 0);
	const double deuxPeriodesNs = 2.0 * periodeMs * 1e6;

	// Une minuterie fine, dans les deux mesures : le verrou en a besoin pour attendre juste.
	NkChrono::BeginPreciseTiming();
	std::printf("copies de la salle : %llu\n", (unsigned long long)copies);
	std::printf("detail : ecran %g Hz, periode %.3f ms, verrou a deux periodes %.3f ms (%s), minuterie fine %s\n",
				hz, periodeMs, 2.0 * periodeMs, verrou ? "actif" : "inactif",
				NkChrono::IsPreciseTimingActive() ? "demandee" : "non demandee");

	NkWindowConfig cfg;
	cfg.title = "Ch07 Exo9 - le verrou";
	cfg.width = 1280;
	cfg.height = 720;
	cfg.centered = true;
	cfg.vsync = true;

	NkWindow fenetre(cfg);
	if (!fenetre.IsValid()) {
		std::printf("[MaSalle] la fenetre n'a pas pu s'ouvrir\n");
		return 1;
	}

	NkDeviceInitInfo devInit{};
	devInit.surface = fenetre.GetSurfaceDesc();
	devInit.width = fenetre.GetSize().x;
	devInit.height = fenetre.GetSize().y;

	NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInit);
	if (device == nullptr) {
		std::printf("[MaSalle] aucune API graphique disponible\n");
		return 1;
	}

	// ── Deux renderers, et c'est voulu ─────────────────────────────────────
	//
	// Le premier POSSÈDE l'image : c'est lui qui appelle BeginFrame, Present et
	// EndFrame, et il ne dessine que du 2D.
	//
	// Le second dessine la salle, mais PAS à l'écran : dans une cible hors
	// écran, une texture qu'on peut relire. On affiche ensuite cette texture
	// dans la fenêtre, et on peut l'enregistrer en PNG.
	//
	// Pourquoi ce détour, alors que dessiner directement à l'écran marcherait ?
	// Deux raisons, et la seconde est celle qui compte pour ce livre :
	//   - une chaîne d'échange ne se relit pas, donc on ne peut pas la capturer,
	//     donc on ne peut pas vérifier une image autrement qu'à l'œil ;
	//   - au chapitre 9, il faudra dessiner DEUX fois, dans deux cibles
	//     différentes. Autant monter la machinerie maintenant, avec une seule.
	//
	// Un renderer par cible, et non une cible qu'on change : SetFinalColorTarget
	// reconstruit le graphe de rendu, et le faire à chaque image est banni.
	NkRendererConfig cfgEcran =
		NkRendererConfig::For2D(devInit.api, devInit.width, devInit.height);
	NkRenderer *rEcran = NkRenderer::Create(device, cfgEcran);
	if (rEcran == nullptr || !rEcran->Initialize()) {
		std::printf("[MaSalle] le renderer d'ecran n'a pas pu s'initialiser\n");
		NkDeviceFactory::Destroy(device);
		return 1;
	}

	NkOffscreenDesc od;
	od.width = devInit.width;
	od.height = devInit.height;
	od.hasDepth = true;
	od.hdr = false;
	od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
	od.readable = true;
	od.readback = true; // sans quoi Capture() ne peut rien relire
	od.name = "MaSalle_Cible";

	NkOffscreenTarget *cible = rEcran->CreateOffscreen(od);
	if (cible == nullptr) {
		std::printf("[MaSalle] cible hors ecran refusee\n");
		NkRenderer::Destroy(rEcran);
		NkDeviceFactory::Destroy(device);
		return 1;
	}

	NkRendererConfig cfgSalle =
		NkRendererConfig::ForGame(devInit.api, devInit.width, devInit.height);
	cfgSalle.Disable(NK_SS_RENDER2D);
	cfgSalle.Disable(NK_SS_TEXT);
	cfgSalle.Disable(NK_SS_OVERLAY);
	cfgSalle.Enable(NK_SS_OFFSCREEN);

	// NK_SS_POST_PROCESS porte le tonemap, le bloom, le FXAA et le SSAO.
	// MASALLE_POST=0 le coupe : c'est l'expérience qui dit si un défaut
	// d'image vient de la scène ou d'un effet écran. Voir le journal de bord.
	if (EnvU64("MASALLE_POST", 1) == 0) {
		cfgSalle.Disable(NK_SS_POST_PROCESS);
	}

	NkRenderer *renderer = NkRenderer::Create(device, cfgSalle);
	if (renderer == nullptr || !renderer->Initialize()) {
		std::printf("[MaSalle] le renderer de la salle n'a pas pu s'initialiser\n");
		NkRenderer::Destroy(rEcran);
		NkDeviceFactory::Destroy(device);
		return 1;
	}
	renderer->SetFinalColorTarget(
		rEcran->GetTextures()->GetRHIHandle(cible->GetColorResult()));

	std::printf("api : %s\n", NkGraphicsApiName(devInit.api));
	std::printf("[MaSalle] salle de %.1f m sur %.1f m, plafond a %.2f m\n",
				(double)kPiece, (double)kPiece, (double)kPlafond);

	// Sortie automatique, pour que la construction se vérifie sans main
	// humaine : MASALLE_EXIT=<image> arrête proprement.
	const uint64 imageDeSortie = EnvU64("MASALLE_EXIT", 0);
	const uint64 imageDeCapture = EnvU64("MASALLE_SHOT", 0);

	uint64 image = 0;
	bool tourne = true;
	std::vector<double> debutsNs;   // le début de chaque image, en nanosecondes
	debutsNs.reserve(2048);
	size_t imagesVerrouAttendues = 0;	 // images où le verrou a vraiment attendu
	size_t imagesVerrouTropLongues = 0;	 // images déjà plus longues que deux périodes : rien à attendre
	double attenteTotaleMs = 0.0;

	while (tourne && fenetre.IsOpen()) {
		// Le début de l'image, noté tout de suite : l'horloge ne revient jamais en arrière.
		debutsNs.push_back(NkChrono::Now().nanoseconds);

		// Les mille intervalles sont faits : ce début d'image est le dernier, la salle s'arrête.
		if (debutsNs.size() >= IMAGES_IGNOREES + INTERVALLES + 1) {
			break;
		}

		NkEvent *ev = nullptr;
		while (NkEvents().PollEvent(ev)) {
			if (ev->As<NkWindowCloseEvent>() != nullptr) {
				tourne = false;
			}
		}
		if (!tourne) break;

		if (rEcran->BeginFrame()) {
			NkICommandBuffer *cmd = rEcran->GetCmd();

			NkRender3D *r3d = renderer->GetRender3D();

			// Ce que BeginFrame ferait si ce renderer possédait l'image. En
			// mode partagé, on le rejoue à la main. Oublier ResetFrame piétine
			// les tampons uniformes d'une image à l'autre.
			renderer->FlushGraphRebuilds();
			r3d->ResetFrame();
			if (auto *mc = renderer->GetMaterialCollection()) {
				mc->Upload();
			}

			NkCamera3DData cd;
			// À hauteur d'yeux, un mètre en avant du mur du fond, regardant
			// la porte. Une caméra fixe : la tête arrive au chapitre 7.
			cd.position = { 0.30f, kYeux, 1.20f };
			cd.target = { -0.70f, 1.10f, -kDemi };
			cd.up = { 0.f, 1.f, 0.f };
			cd.fovY = 70.f;
			cd.aspect = (float32)devInit.width / (float32)devInit.height;
			cd.nearPlane = 0.05f;
			cd.farPlane = 100.f;

			NkSceneContext sctx;
			sctx.camera = NkCamera3D(cd);
			sctx.time = (float32)image / 60.f;
			// La part de lumière qui vient du ciel (0 = aucune, 1 = toute).
			r3d->SetIBLStrength(0.35f);

			NkLightDesc soleil;
			soleil.type = NkLightType::NK_DIRECTIONAL;
			soleil.direction = { -0.5f, -1.f, -0.35f };
			soleil.color = { 1.f, 0.96f, 0.88f };
			soleil.intensity = 3.f;
			soleil.castShadow = false;
			sctx.lights.PushBack(soleil);

			r3d->BeginScene(sctx);
			// La salle, plusieurs fois : chaque copie décalée de 5 mètres le long de x.
			for (uint64 c = 0; c < copies; ++c) {
				gDecalageX = 5.0f * (float32)c;
				SoumettreLaSalle(r3d, renderer->GetMeshSystem());
			}
			gDecalageX = 0.f;

			// Le graphe de la salle s'exécute sur le command buffer de l'écran,
			// AVANT son Present : deux passes imbriquées sont interdites.
			if (auto *graph = renderer->GetRenderGraph()) {
				graph->Execute(cmd);
			}

			// La texture, montrée dans la fenêtre. C'est tout ce que fait le
			// renderer d'écran : recopier une image déjà faite.
			if (auto *r2d = rEcran->GetRender2D()) {
				r2d->DrawImage(cible->GetColorResult(),
							   { 0.f, 0.f, (float32)devInit.width,
								 (float32)devInit.height });
			}

			rEcran->Present();
			rEcran->EndFrame();
		}

		++image;

		// La capture, à une image nommée : déterministe, reproductible, et
		// c'est elle qui fait les figures du livre.
		if (imageDeCapture != 0 && image == imageDeCapture) {
			const char *chemin = getenv("MASALLE_SHOT_PATH");
			if (chemin == nullptr || *chemin == '\0') {
				chemin = "masalle.png";
			}
			const bool ok = cible->Capture(chemin);
			std::printf("[MaSalle] capture image %llu -> %s (%s)\n",
						(unsigned long long)image, chemin, ok ? "ok" : "echec");
		}

		// Le verrou : à la fin de l'image, attendre que deux périodes d'écran se soient écoulées
		// depuis son DÉBUT. Si le travail a déjà pris plus, il n'y a rien à attendre.
		if (verrou) {
			const double cibleNs = debutsNs.back() + deuxPeriodesNs;
			const double avantNs = NkChrono::Now().nanoseconds;
			if (avantNs >= cibleNs) {
				++imagesVerrouTropLongues;
			} else {
				AttendreJusqua(cibleNs);
				++imagesVerrouAttendues;
				attenteTotaleMs += (NkChrono::Now().nanoseconds - avantNs) * 1e-6;
			}
		}

		if (imageDeSortie != 0 && image >= imageDeSortie) {
			tourne = false;
		}
	}

	std::printf("[MaSalle] %llu images\n", (unsigned long long)image);

	// La mesure : mille intervalles entre deux débuts d'image.
	std::vector<double> intervallesMs;
	if (LesIntervalles(debutsNs, intervallesMs)) {
		ImprimerLaMesure(verrou ? "avec verrou" : "sans verrou", intervallesMs);
		if (verrou) {
			std::printf("detail verrou : %zu images ont attendu (%.3f ms en moyenne), %zu etaient deja plus longues que deux periodes\n",
						imagesVerrouAttendues,
						imagesVerrouAttendues ? attenteTotaleMs / (double)imagesVerrouAttendues : 0.0,
						imagesVerrouTropLongues);
		}
	} else {
		std::printf("[ERREUR] il faut %llu images pour mesurer : laissez la salle ouverte jusqu'au bout.\n",
					(unsigned long long)(IMAGES_IGNOREES + INTERVALLES + 1));
	}
	NkChrono::EndPreciseTiming();

	NkRenderer::Destroy(renderer);
	NkRenderer::Destroy(rEcran);
	NkDeviceFactory::Destroy(device);
	return 0;
}
