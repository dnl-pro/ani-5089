// =============================================================================
//  Chapitre 7, exercice 10 — un meilleur modèle ?
//
//  Au démarrage, avant d'ouvrir la fenêtre : deux modèles de prédiction du lacet (en degrés,
//  sans quaternion), une période de 72 hertz à l'avance, comparés sur quatre gestes.
//  - Le modèle à vitesse constante : lacet + vitesse * période, la vitesse tirée des deux
//    dernières poses.
//  - Le modèle à accélération : il y ajoute accélération * période * période / 2, l'accélération
//    tirée des trois dernières poses.
//  - Les gestes : un lent, le quart de tour en 400 ms, un demi-tour soudain (tous trois avec la
//    forme douce du quart de tour : départ et arrivée en douceur) et un arrêt, où la tête tourne
//    à vitesse constante puis s'arrête morte.
//  - Pour chaque geste et chaque modèle : l'erreur moyenne et la plus grande, en degrés, entre le
//    lacet prédit et le vrai lacet une période plus tard, pendant le geste et les 200 ms qui suivent.
//  Une ligne par geste :
//      nom (A deg en D ms) : vitesse V deg (max X), acceleration A deg (max Y)
//  MASALLE_EXIT=1 arrête la salle d'elle-même après une image.
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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace nkentseu;
using namespace nkentseu::renderer;

static void ConfigureAppData(NkAppData &d) { d.appName = "Ch07Exo10"; }
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

	// Un pavé posé dans la salle. `centre` est le centre géométrique, `taille`
	// les trois dimensions en mètres : on ne manipule jamais autre chose.
	void Pave(NkRender3D *r3d, NkMeshSystem *meshes, const NkVec3f &centre,
			  const NkVec3f &taille, const NkVec3f &teinte, float32 rugosite = 1.f) {
		NkDrawCall3D dc;
		dc.mesh = meshes->GetCube();
		dc.transform = NkMat4f::Translate(centre) * NkMat4f::Scale(taille);
		dc.aabb = { { centre.x - taille.x * 0.5f, centre.y - taille.y * 0.5f,
					  centre.z - taille.z * 0.5f },
					{ centre.x + taille.x * 0.5f, centre.y + taille.y * 0.5f,
					  centre.z + taille.z * 0.5f } };
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
			dc.transform = NkMat4f::Scale({ kPiece, 1.f, kPiece });
			dc.aabb = { { -kDemi, 0.f, -kDemi }, { kDemi, 0.f, kDemi } };
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

	// ── Les gestes et les modèles (le lacet seul, en degrés, sans quaternion) ──
	const double kPi = 3.14159265358979323846;
	const double kHz = 72.0;			// la cadence d'un casque
	const double kPeriode = 1.0 / kHz;	// une pose par période

	enum class Forme { Douce, Arret };

	// Un geste : une amplitude (degrés) et une durée (secondes).
	struct Geste {
		const char *nom;
		Forme forme;
		double amplitude;
		double duree;
	};

	// Un geste en douceur : départ et arrivée doux (celui de l'énoncé : 90 degrés en 400 ms).
	// t en secondes depuis le début, lacet en degrés.
	double LacetDoux(double t, double amplitude, double duree) {
		if (t <= 0.0) return 0.0;
		if (t >= duree) return amplitude;
		return amplitude * 0.5 * (1.0 - std::cos(kPi * t / duree));
	}

	// Un arrêt : la tête tourne à vitesse constante (amplitude / durée), déjà avant t = 0,
	// puis s'arrête morte à t = durée : la vitesse passe de sa valeur à zéro d'un coup.
	double LacetArret(double t, double amplitude, double duree) {
		const double vitesse = amplitude / duree;
		return t >= duree ? amplitude : vitesse * t;
	}

	double LacetDe(const Geste &g, double t) {
		return g.forme == Forme::Arret ? LacetArret(t, g.amplitude, g.duree)
									   : LacetDoux(t, g.amplitude, g.duree);
	}

	// Le modèle à vitesse constante : la vitesse vient des deux dernières poses.
	//   prédit = lacet + vitesse * période
	double PrevoirVitesse(double y0, double y1) { // y1 : la dernière pose, y0 : celle d'avant
		const double vitesse = (y1 - y0) / kPeriode;
		return y1 + vitesse * kPeriode;
	}

	// Le modèle à accélération : l'accélération vient des trois dernières poses.
	//   prédit = lacet + vitesse * période + accélération * période * période / 2
	// L'accélération est une différence seconde divisée par le carré de la période : une
	// erreur d'un degré sur une pose devient plus de cinq mille degrés par seconde au carré.
	double PrevoirAcceleration(double ym2, double ym1, double y) { // y : la dernière pose
		const double vitesse = (y - ym1) / kPeriode;
		const double acceleration = (y - 2.0 * ym1 + ym2) / (kPeriode * kPeriode);
		return y + vitesse * kPeriode + acceleration * kPeriode * kPeriode / 2.0;
	}

	struct Erreurs {
		double somme = 0.0;
		double maximum = 0.0;
		double tMaximum = 0.0;
		void Ajouter(double e, double t) {
			somme += e;
			if (e > maximum) { maximum = e; tMaximum = t; }
		}
	};

	struct Resultat {
		double moyVitesse, maxVitesse, moyAcceleration, maxAcceleration;
	};

	// Un enregistrement : une pose par période, de t = 0 jusqu'à la fin du geste et 200 ms de plus.
	// Les deux modèles prédisent le MÊME instant (la pose suivante) à partir des MÊMES poses.
	Resultat MesurerUnGeste(const Geste &g, bool detail, bool imprimer = true) {
		const double fin = g.duree + 0.200;
		Erreurs vitesse, acceleration;
		size_t n = 0;
		for (size_t k = 0; (double)k * kPeriode <= fin + 1e-9; ++k) {
			const double t = (double)k * kPeriode;
			const double y = LacetDe(g, t);
			const double ym1 = LacetDe(g, t - kPeriode);
			const double ym2 = LacetDe(g, t - 2.0 * kPeriode);
			const double vrai = LacetDe(g, t + kPeriode); // la pose à l'instant d'affichage
			const double errV = std::fabs(PrevoirVitesse(ym1, y) - vrai);
			const double errA = std::fabs(PrevoirAcceleration(ym2, ym1, y) - vrai);
			vitesse.Ajouter(errV, t);
			acceleration.Ajouter(errA, t);
			if (detail && (errV > 0.001 || errA > 0.001)) {
				std::printf("detail %s : t = %.1f ms, lacet %.4f, vrai %.4f, erreur vitesse %.4f deg, "
							"erreur acceleration %.4f deg\n",
							g.nom, t * 1000.0, y, vrai, errV, errA);
			}
			++n;
		}
		const double moyV = vitesse.somme / (double)n;
		const double moyA = acceleration.somme / (double)n;
		if (!imprimer) {
			return { moyV, vitesse.maximum, moyA, acceleration.maximum };
		}
		std::printf("%s (%g deg en %g ms) : vitesse %.4f deg (max %.4f), acceleration %.4f deg (max %.4f)\n",
					g.nom, g.amplitude, g.duree * 1000.0, moyV, vitesse.maximum, moyA, acceleration.maximum);
		std::printf("detail %s : %zu poses, max vitesse a t = %.1f ms, max acceleration a t = %.1f ms, "
					"moyenne gagnee par %s, maximum gagne par %s\n",
					g.nom, n, vitesse.tMaximum * 1000.0, acceleration.tMaximum * 1000.0,
					moyA < moyV ? "l'acceleration" : "la vitesse",
					acceleration.maximum < vitesse.maximum ? "l'acceleration" : "la vitesse");
		return { moyV, vitesse.maximum, moyA, acceleration.maximum };
	}

	// L'arrêt tombe à un instant qui dépend de la phase par rapport aux poses (une pose toutes
	// les 13,9 ms). Un seul arrêt est donc un seul tirage : on refait la mesure pour vingt
	// phases, de 500 ms à une période de plus, pour voir ce que donne un arrêt quelconque.
	void BalayerLesPhasesDeLArret() {
		const int phases = 20;
		double sommeMoyV = 0.0, sommeMoyA = 0.0, sommeMaxV = 0.0, sommeMaxA = 0.0;
		int moyenneAcceleration = 0, maximumAcceleration = 0;
		for (int i = 0; i < phases; ++i) {
			const Geste g = { "arret", Forme::Arret, 90.0, 0.500 + kPeriode * (double)i / (double)phases };
			const Resultat r = MesurerUnGeste(g, false, false);
			sommeMoyV += r.moyVitesse;
			sommeMoyA += r.moyAcceleration;
			sommeMaxV += r.maxVitesse;
			sommeMaxA += r.maxAcceleration;
			if (r.moyAcceleration < r.moyVitesse - 1e-9) ++moyenneAcceleration;
			if (r.maxAcceleration < r.maxVitesse - 1e-9) ++maximumAcceleration;
		}
		std::printf("detail arret, %d phases (de 500 ms a 500 ms + une periode) : moyenne des erreurs moyennes : "
					"vitesse %.4f, acceleration %.4f ; moyenne des maxima : vitesse %.4f, acceleration %.4f ; "
					"l'acceleration gagne la moyenne dans %d phases et le maximum dans %d\n",
					phases, sommeMoyV / phases, sommeMoyA / phases, sommeMaxV / phases, sommeMaxA / phases,
					moyenneAcceleration, maximumAcceleration);
	}

	// Les quatre gestes : le rapide, le lent, le demi-tour et l'arrêt, dans cet ordre.
	void MesurerLesModeles() {
		const Geste gestes[4] = {
			{ "rapide", Forme::Douce, 90.0, 0.400 },
			{ "lent", Forme::Douce, 45.0, 1.000 },
			{ "demi-tour", Forme::Douce, 180.0, 0.250 },
			{ "arret", Forme::Arret, 90.0, 0.500 }, // 180 deg/s pendant 500 ms, puis arrêt mort
		};
		for (int i = 0; i < 4; ++i) {
			MesurerUnGeste(gestes[i], gestes[i].forme == Forme::Arret);
		}
		BalayerLesPhasesDeLArret();
	}

} // namespace

int nkmain(const NkEntryState &state) {
	(void)state;

	// Les modèles : au démarrage, avant d'ouvrir la fenêtre.
	MesurerLesModeles();

	NkWindowConfig cfg;
	cfg.title = "Ch07 Exo10 - un meilleur modele";
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

	std::printf("[MaSalle] salle de %.1f m sur %.1f m, plafond a %.2f m\n",
				(double)kPiece, (double)kPiece, (double)kPlafond);

	// Sortie automatique, pour que la construction se vérifie sans main
	// humaine : MASALLE_EXIT=<image> arrête proprement.
	const uint64 imageDeSortie = EnvU64("MASALLE_EXIT", 0);
	const uint64 imageDeCapture = EnvU64("MASALLE_SHOT", 0);

	uint64 image = 0;
	bool tourne = true;

	while (tourne && fenetre.IsOpen()) {
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
			SoumettreLaSalle(r3d, renderer->GetMeshSystem());

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

		if (imageDeSortie != 0 && image >= imageDeSortie) {
			tourne = false;
		}
	}

	std::printf("[MaSalle] %llu images\n", (unsigned long long)image);

	NkRenderer::Destroy(renderer);
	NkRenderer::Destroy(rEcran);
	NkDeviceFactory::Destroy(device);
	return 0;
}
