// =============================================================================
//  Chapitre 7, exercice 5 — prédire, et comparer.
//
//  La salle ci-dessous (étape 5 du kit) devient une salle où l'on tourne la tête à la souris.
//  - La souris : les mouvements bruts (NkMouseRawEvent) sont accumulés ; UNE fois par image,
//    on retire du lacet le total x * 0,0022 rad, du tangage le total y * 0,0022 rad, puis on
//    vide l'accumulateur.
//  - L'orientation de la tête : deux matrices, puis une conversion en quaternion normalisé.
//  - À chaque image : la vitesse angulaire entre la pose précédente et celle-ci
//    (VitesseAngulaire, celle du module de réalité virtuelle du moteur), puis la pose PRÉDITE
//    pour l'instant où l'image sera vue : l'orientation tournée à cette vitesse pendant une
//    période de l'écran (Extrapoler). MASALLE_HZ donne le taux de rafraîchissement de l'écran
//    (60 par défaut) ; la période est 1 / MASALLE_HZ.
//  - À l'image SUIVANTE, on mesure en degrés l'angle entre la pose prédite gardée et la pose
//    mesurée, et l'angle entre l'ancienne pose mesurée, prise telle qu'elle est, et la
//    nouvelle : l'erreur sans prédiction. Les deux erreurs portent sur les mêmes images.
//  - On compte mille comparaisons, à partir de la première image où la souris bouge (après
//    les trente premières images), puis le programme s'arrête et imprime :
//        avec prediction : moyenne M deg, max X deg, sur N images
//        sans prediction : moyenne M deg, max X deg, sur N images
//    suivies de quelques lignes de contexte (images où la tête ne bouge pas, période, dt).
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
#include "NKEvent/NkMouseEvent.h"
#include "NKMath/NkQuat.h"

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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace nkentseu;
using namespace nkentseu::renderer;
static void ConfigureAppData(NkAppData &d) { d.appName = "Ch07Exo5"; }
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

	double EnvDouble(const char *nom, double defaut) {
		const char *t = getenv(nom);
		if (t == nullptr || *t == '\0') return defaut;
		return strtod(t, nullptr);
	}

	// ── La tête ───────────────────────────────────────────────────────────────
	const float32 kRadParCompte = 0.0022f; // radian par compte de souris
	const float32 kTangageMax = 1.5f;	   // on ne se retourne pas la tête
	const double kPi = 3.14159265358979323846;
	const uint64 kImagesIgnorees = 30;	   // la première image paie le démarrage
	const size_t kComparaisons = 1000;	   // mille images comparées

	// Le quaternion de la tête : deux matrices, puis une conversion.
	NkQuatf OrientationDeLaTete(float32 lacetRad, float32 tangageRad) {
		const NkMat4f rotation = NkMat4f::RotationY(NkAngle::FromRad(lacetRad)) *
								 NkMat4f::RotationX(NkAngle::FromRad(tangageRad));
		return NkQuatf(rotation).Normalized();
	}

	// La vitesse angulaire entre deux poses chronométrées (rad/s, axe * vitesse).
	NkVec3f VitesseAngulaire(const NkQuatf &from, const NkQuatf &to, float32 dtSeconds) {
		if (dtSeconds <= 0.f) {
			return NkVec3f(0.f, 0.f, 0.f);
		}
		NkQuatf delta = (to * from.Conjugate()).Normalized();
		// Les deux hemispheres du quaternion codent la meme rotation ; on
		// force le chemin court, sinon un delta minuscule peut se lire
		// comme un tour presque complet dans l'autre sens.
		if (delta.w < 0.f) {
			delta = NkQuatf(-delta.x, -delta.y, -delta.z, -delta.w);
		}
		const float32 angleRad = delta.Angle().Rad();
		if (angleRad <= 1e-6f) {
			return NkVec3f(0.f, 0.f, 0.f);
		}
		return delta.Axis() * (angleRad / dtSeconds);
	}

	// Prédire : tourner l'orientation à cette vitesse pendant le temps laissé avant l'affichage.
	NkQuatf Extrapoler(const NkQuatf &orientation, const NkVec3f &vitesse, float32 dtSeconds) {
		const float32 speedRad = vitesse.Len();
		if (speedRad > 1e-6f) {
			const NkVec3f axis = vitesse * (1.f / speedRad);
			// Delta en espace de BASE : il se compose a GAUCHE de
			// l'orientation courante (meme convention que la mesure).
			const NkQuatf delta(NkAngle::FromRad(speedRad * dtSeconds), axis);
			return (delta * orientation).Normalized();
		}
		return orientation;
	}

	// L'angle, en degrés, entre deux orientations.
	double AngleEntre(const NkQuatf &a, const NkQuatf &b) {
		return (double)(b * a.Conjugate()).Normalized().Angle().Deg();
	}

} // namespace

int nkmain(const NkEntryState &state) {
	(void)state;

	NkWindowConfig cfg;
	cfg.title = "Ch07 Exo5 - predire et comparer";
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

	// L'écran : MASALLE_HZ est son taux de rafraîchissement, la période est 1 / MASALLE_HZ.
	const double hz = EnvDouble("MASALLE_HZ", 60.0);
	const double periodeS = 1.0 / (hz > 1.0 ? hz : 60.0);
	const float64 depart = NkChrono::Now().nanoseconds;

	float32 lacetRad = 0.f;
	float32 tangageRad = 0.f;
	int32 cumulX = 0; // l'accumulateur de la souris, vidé à chaque image
	int32 cumulY = 0;

	NkQuatf precedente = NkQuatf::Identity(); // la pose mesurée de l'image d'avant
	NkQuatf predite = NkQuatf::Identity();	  // la pose prédite à l'image d'avant
	double precedenteS = 0.0;
	bool aPrecedente = false;
	bool aPrediction = false;
	bool commence = false; // la mesure commence au premier mouvement de souris

	std::vector<double> erreursAvec;	// en degrés, une par image comparée
	std::vector<double> erreursSans;
	std::vector<char> immobile;			// 1 si la souris n'a pas bougé à cette image
	double sommeDt = 0.0;

	while (tourne && fenetre.IsOpen()) {
		// L'instant de l'image, noté tout de suite, en secondes depuis le démarrage.
		const double maintenantS = (NkChrono::Now().nanoseconds - depart) * 1e-9;

		NkEvent *ev = nullptr;
		while (NkEvents().PollEvent(ev)) {
			if (ev->As<NkWindowCloseEvent>() != nullptr) {
				tourne = false;
			}
			if (auto *brut = ev->As<NkMouseRawEvent>()) {
				cumulX += brut->GetDeltaX();
				cumulY += brut->GetDeltaY();
			}
		}
		if (!tourne) break;

		// La souris, lue une fois par image : le total accumulé, puis on vide.
		const bool aBouge = (cumulX != 0 || cumulY != 0);
		lacetRad -= (float32)cumulX * kRadParCompte;
		tangageRad -= (float32)cumulY * kRadParCompte;
		if (tangageRad > kTangageMax) tangageRad = kTangageMax;
		if (tangageRad < -kTangageMax) tangageRad = -kTangageMax;
		cumulX = 0;
		cumulY = 0;

		const NkQuatf orientation = OrientationDeLaTete(lacetRad, tangageRad);

		// 1. L'image d'avant avait prédit cette pose : on compare, avec la pose mesurée ICI
		//    (l'image suivante de la prédiction, pas la même image).
		if (aPrediction) {
			if (!commence && image >= kImagesIgnorees && aBouge) {
				commence = true;
			}
			if (commence && erreursAvec.size() < kComparaisons) {
				erreursAvec.push_back(AngleEntre(predite, orientation));
				erreursSans.push_back(AngleEntre(precedente, orientation)); // sans prédiction
				immobile.push_back(aBouge ? 0 : 1);
				sommeDt += maintenantS - precedenteS;
			}
		}

		// 2. La vitesse entre la pose précédente et celle-ci, puis la nouvelle prédiction,
		//    pour l'instant où cette image sera vue : une période d'écran plus tard.
		NkVec3f vitesse(0.f, 0.f, 0.f);
		if (aPrecedente) {
			vitesse = VitesseAngulaire(precedente, orientation, (float32)(maintenantS - precedenteS));
		}
		predite = Extrapoler(orientation, vitesse, (float32)periodeS);
		aPrediction = true;
		precedente = orientation;
		precedenteS = maintenantS;
		aPrecedente = true;

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
			// À hauteur d'yeux, au même endroit qu'avant, mais la tête tourne :
			// la caméra suit l'orientation.
			cd.position = { 0.30f, kYeux, 1.20f };
			cd.target = cd.position + orientation * NkVec3f(0.f, 0.f, -1.f);
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
		if (erreursAvec.size() >= kComparaisons) {
			tourne = false; // les mille comparaisons sont faites
		}
	}

	std::printf("[MaSalle] %llu images\n", (unsigned long long)image);

	// Les deux erreurs : moyenne et maximum de chacune, sur les mêmes images.
	if (erreursAvec.size() >= kComparaisons) {
		const size_t n = erreursAvec.size();
		double sAvec = 0.0, sSans = 0.0, mAvec = 0.0, mSans = 0.0;
		double sAvecMvt = 0.0, sSansMvt = 0.0;
		size_t nImmobiles = 0;
		for (size_t i = 0; i < n; ++i) {
			sAvec += erreursAvec[i];
			sSans += erreursSans[i];
			if (erreursAvec[i] > mAvec) mAvec = erreursAvec[i];
			if (erreursSans[i] > mSans) mSans = erreursSans[i];
			if (immobile[i]) {
				++nImmobiles;
			} else {
				sAvecMvt += erreursAvec[i];
				sSansMvt += erreursSans[i];
			}
		}
		std::printf("avec prediction : moyenne %.4f deg, max %.4f deg, sur %zu images\n", sAvec / (double)n, mAvec, n);
		std::printf("sans prediction : moyenne %.4f deg, max %.4f deg, sur %zu images\n", sSans / (double)n, mSans, n);
		std::printf("images immobiles : %zu sur %zu\n", nImmobiles, n);
		if (n > nImmobiles) {
			const double m = (double)(n - nImmobiles);
			std::printf("en mouvement seulement : avec prediction %.4f deg, sans prediction %.4f deg, sur %zu images\n",
						sAvecMvt / m, sSansMvt / m, n - nImmobiles);
		}
		std::printf("periode = %.3f ms (MASALLE_HZ = %g), dt moyen entre images = %.3f ms\n",
					periodeS * 1000.0, hz, sommeDt / (double)n * 1000.0);
	} else {
		std::printf("[ERREUR] il faut %zu comparaisons : bougez la souris, en continu, apres les 30 premieres images.\n",
					kComparaisons);
	}

	NkRenderer::Destroy(renderer);
	NkRenderer::Destroy(rEcran);
	NkDeviceFactory::Destroy(device);
	return 0;
}
