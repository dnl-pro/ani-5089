// =============================================================================
//  Chapitre 7, exercice 4 — le chemin long, provoqué.
//
//  La salle ci-dessous (étape 5 du kit) devient une salle où la tête tourne toute seule.
//  - L'orientation de la tête est construite comme dans le simulateur du moteur :
//    deux matrices, puis une conversion en quaternion normalisé.
//  - MASALLE_TOUR=<deg/s> : le lacet suit l'horloge, depuis la première image.
//    Avec MASALLE_TOUR=20, la tête fait un tour complet (360 degrés) en 18 secondes,
//    puis le programme s'arrête de lui-même.
//  - VitesseAngulaire() est celle du module de réalité virtuelle du moteur, MOINS le
//    test sur le signe de w (et son bloc), qui forçait le chemin court. Rien d'autre
//    ne change dans le calcul.
//  - À chaque image, sans en sauter aucune, on imprime une ligne :
//        image N : yaw = L deg, vitesse = (x, y, z) deg/s, delta.w = W
//    où W est la composante w du quaternion delta utilisé par le calcul.
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
static void ConfigureAppData(NkAppData &d) { d.appName = "Ch07Exo4"; }
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
	const double kPi = 3.14159265358979323846;

	// Le quaternion de la tête : deux matrices, puis une conversion.
	NkQuatf OrientationDeLaTete(float32 lacetRad, float32 tangageRad) {
		const NkMat4f rotation = NkMat4f::RotationY(NkAngle::FromRad(lacetRad)) *
								 NkMat4f::RotationX(NkAngle::FromRad(tangageRad));
		return NkQuatf(rotation).Normalized();
	}

	// La vitesse angulaire entre deux poses chronométrées (rad/s, axe * vitesse).
	// Le test sur le signe de w, qui forçait le chemin court, est retiré.
	NkVec3f VitesseAngulaire(const NkQuatf &from, const NkQuatf &to, float32 dtSeconds) {
		if (dtSeconds <= 0.f) {
			return NkVec3f(0.f, 0.f, 0.f);
		}
		NkQuatf delta = (to * from.Conjugate()).Normalized();
		const float32 angleRad = delta.Angle().Rad();
		if (angleRad <= 1e-6f) {
			return NkVec3f(0.f, 0.f, 0.f);
		}
		return delta.Axis() * (angleRad / dtSeconds);
	}

	struct LigneDeChemin {
		uint64 image;
		double yawDeg;
		double x, y, z; // degrés par seconde
		double deltaW;
	};

} // namespace

int nkmain(const NkEntryState &state) {
	(void)state;

	NkWindowConfig cfg;
	cfg.title = "Ch07 Exo4 - le chemin long";
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

	// Le tour scripté : MASALLE_TOUR est une vitesse en degrés par seconde.
	const double tourDegS = EnvDouble("MASALLE_TOUR", 0.0);
	if (tourDegS <= 0.0) {
		std::printf("[ERREUR] lancez avec MASALLE_TOUR=20 (degres par seconde).\n");
		tourne = false;
	}
	float64 depart = 0.0; // l'horloge du tour démarre à la première image

	NkQuatf precedente = NkQuatf::Identity();
	double precedenteS = 0.0;
	bool aPrecedente = false;
	bool tourComplet = false;

	std::vector<LigneDeChemin> lignes;

	while (tourne && fenetre.IsOpen()) {
		// L'instant de l'image, noté tout de suite, en secondes depuis la première image.
		const float64 maintenantNs = NkChrono::Now().nanoseconds;
		if (!aPrecedente) {
			depart = maintenantNs;
		}
		const double maintenantS = (maintenantNs - depart) * 1e-9;

		NkEvent *ev = nullptr;
		while (NkEvents().PollEvent(ev)) {
			if (ev->As<NkWindowCloseEvent>() != nullptr) {
				tourne = false;
			}
		}
		if (!tourne) break;

		// Le lacet suit l'horloge ; pas de souris, pas de tangage.
		const double yawDeg = tourDegS * maintenantS;
		const float32 lacetRad = (float32)(yawDeg * kPi / 180.0);
		const NkQuatf orientation = OrientationDeLaTete(lacetRad, 0.f);

		// La vitesse entre la pose précédente et celle-ci, pour CHAQUE image.
		if (aPrecedente) {
			const double dtS = maintenantS - precedenteS;
			const NkVec3f v = VitesseAngulaire(precedente, orientation, (float32)dtS);
			const NkQuatf delta = (orientation * precedente.Conjugate()).Normalized();
			const double enDegres = 180.0 / kPi;
			lignes.push_back({ image, yawDeg, (double)v.x * enDegres, (double)v.y * enDegres,
							   (double)v.z * enDegres, (double)delta.w });
		}
		precedente = orientation;
		precedenteS = maintenantS;
		aPrecedente = true;
		if (yawDeg >= 360.0) {
			tourComplet = true; // un tour complet : cette image est la dernière
		}

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
		if (tourComplet) {
			tourne = false;
		}
	}

	std::printf("[MaSalle] %llu images\n", (unsigned long long)image);

	// Une ligne par image, rien de sauté.
	if (tourComplet) {
		for (const LigneDeChemin &l : lignes) {
			std::printf("image %llu : yaw = %.3f deg, vitesse = (%.3f, %.3f, %.3f) deg/s, delta.w = %.6f\n",
						(unsigned long long)l.image, l.yawDeg, l.x, l.y, l.z, l.deltaW);
		}
	} else if (tourDegS > 0.0) {
		std::printf("[ERREUR] le tour complet n'a pas ete fait : laissez la salle tourner jusqu'au bout.\n");
	}

	NkRenderer::Destroy(renderer);
	NkRenderer::Destroy(rEcran);
	NkDeviceFactory::Destroy(device);
	return 0;
}
