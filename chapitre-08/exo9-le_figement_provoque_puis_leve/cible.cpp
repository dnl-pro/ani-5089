// =============================================================================
//  Exo 9 : le gel de la cible hors ecran, provoque puis leve.
//
//  Un renderer dessine dans une cible hors ecran PLUS PETITE que la fenetre.
//  Relier cette cible demande deux appels :
//      renderer->SetFinalColorTarget(cible);          // OU dessiner
//      renderer->SetRenderSizeOverride(largeur, haut); // EN QUELLE TAILLE
//  Sans le second, la passe garde la taille de la fenetre, travaille sur une
//  zone plus grande que l'image prevue, et le programme se fige sans rien dire.
//
//  Les deux appels sont dans ce fichier. Pour provoquer le gel, on saute le
//  second avec la variable d'environnement CIBLE_SANS_TAILLE=1.
//
//  Chaque image dessinee est inscrite, ligne par ligne, dans le fichier donne
//  par CIBLE_SORTIE (ecrit puis vide a chaque image, donc lisible meme si le
//  programme est fige juste apres).
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
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>

using namespace nkentseu;
using namespace nkentseu::renderer;

static void ConfigureAppData(NkAppData &d) { d.appName = "CibleHorsEcran"; }
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

namespace {

	uint64 EnvU64(const char *nom, uint64 defaut) {
		const char *t = getenv(nom);
		if (t == nullptr || *t == '\0') return defaut;
		return (uint64)strtoull(t, nullptr, 10);
	}

	void SoumettreUnCube(NkRender3D *r3d, NkMeshSystem *meshes) {
		NkDrawCall3D dc;
		dc.mesh = meshes->GetCube();
		dc.transform = NkMat4f::Scale({ 1.f, 1.f, 1.f });
		dc.aabb = { { -0.5f, -0.5f, -0.5f }, { 0.5f, 0.5f, 0.5f } };
		dc.tint = { 0.85f, 0.55f, 0.25f };
		dc.metallic = 0.f;
		dc.roughness = 0.6f;
		r3d->Submit(dc);
	}

	long long MillisecondesMaintenant() {
		return (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
				   std::chrono::steady_clock::now().time_since_epoch())
			.count();
	}

	// Etat partage entre la boucle de rendu et le fil de surveillance.
	std::atomic<unsigned long long> gImageEnCours{0};
	std::atomic<unsigned long long> gImageTerminee{0};
	std::atomic<long long> gDebutImage{0};
	std::atomic<bool> gFini{false};
	std::mutex gMutexSortie;

	void Ecrire(std::FILE *sortie, const char *format, unsigned long long image) {
		if (sortie == nullptr) return;
		std::lock_guard<std::mutex> verrou(gMutexSortie);
		std::fprintf(sortie, format, image);
		std::fflush(sortie);
	}

	// Le fil principal, lui, est bloque : c'est donc un autre fil qui constate
	// le gel. Une image qui dure plus de cinq secondes est declaree gelee.
	void Surveiller(std::FILE *sortie) {
		bool signale = false;
		while (!gFini.load()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			const unsigned long long en = gImageEnCours.load();
			if (!signale && en > gImageTerminee.load() &&
				MillisecondesMaintenant() - gDebutImage.load() > 5000) {
				Ecrire(sortie, "image : %llu, etat : gelee\n", en);
				signale = true;
			}
		}
	}

} // namespace

int nkmain(const NkEntryState &state) {
	(void)state;

	// La fenetre est grande, la cible est plus petite : c'est la condition du
	// gel. Une cible de la taille de la fenetre cacherait l'oubli.
	const uint32 largeurFenetre = 1280;
	const uint32 hauteurFenetre = 720;
	const uint32 largeurCible = (uint32)EnvU64("CIBLE_LARGEUR", 640);
	const uint32 hauteurCible = (uint32)EnvU64("CIBLE_HAUTEUR", 360);

	const bool sansTaille = EnvU64("CIBLE_SANS_TAILLE", 0) != 0;
	const uint64 imageMax = EnvU64("CIBLE_IMAGES", 12);

	const char *cheminSortie = getenv("CIBLE_SORTIE");
	std::FILE *sortie = nullptr;
	if (cheminSortie != nullptr && *cheminSortie != '\0') {
		sortie = std::fopen(cheminSortie, "w");
	}

	NkWindowConfig cfg;
	cfg.title = "Cible hors ecran";
	cfg.width = largeurFenetre;
	cfg.height = hauteurFenetre;
	cfg.centered = true;
	cfg.vsync = true;

	NkWindow fenetre(cfg);
	if (!fenetre.IsValid()) {
		std::fprintf(stderr, "la fenetre n'a pas pu s'ouvrir\n");
		return 1;
	}

	NkDeviceInitInfo devInit{};
	devInit.surface = fenetre.GetSurfaceDesc();
	devInit.width = fenetre.GetSize().x;
	devInit.height = fenetre.GetSize().y;

	NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInit);
	if (device == nullptr) {
		std::fprintf(stderr, "aucune API graphique disponible\n");
		return 1;
	}

	// Le renderer d'ecran possede l'image : il ne fait que montrer la cible.
	NkRendererConfig cfgEcran =
		NkRendererConfig::For2D(devInit.api, devInit.width, devInit.height);
	NkRenderer *rEcran = NkRenderer::Create(device, cfgEcran);
	if (rEcran == nullptr || !rEcran->Initialize()) {
		std::fprintf(stderr, "le renderer d'ecran n'a pas pu s'initialiser\n");
		NkDeviceFactory::Destroy(device);
		return 1;
	}

	NkOffscreenDesc od;
	od.width = largeurCible;
	od.height = hauteurCible;
	od.hasDepth = true;
	od.hdr = false;
	od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
	od.readable = true;
	od.readback = false;
	od.name = "CibleHorsEcran";

	NkOffscreenTarget *cible = rEcran->CreateOffscreen(od);
	if (cible == nullptr) {
		std::fprintf(stderr, "cible hors ecran refusee\n");
		NkRenderer::Destroy(rEcran);
		NkDeviceFactory::Destroy(device);
		return 1;
	}

	// Le renderer qui dessine la scene, dans la cible et pas a l'ecran.
	NkRendererConfig cfgScene =
		NkRendererConfig::ForGame(devInit.api, devInit.width, devInit.height);
	cfgScene.Disable(NK_SS_RENDER2D);
	cfgScene.Disable(NK_SS_TEXT);
	cfgScene.Disable(NK_SS_OVERLAY);
	cfgScene.Enable(NK_SS_OFFSCREEN);

	NkRenderer *renderer = NkRenderer::Create(device, cfgScene);
	if (renderer == nullptr || !renderer->Initialize()) {
		std::fprintf(stderr, "le renderer de la scene n'a pas pu s'initialiser\n");
		NkRenderer::Destroy(rEcran);
		NkDeviceFactory::Destroy(device);
		return 1;
	}

	// Premier appel : OU dessiner.
	renderer->SetFinalColorTarget(
		rEcran->GetTextures()->GetRHIHandle(cible->GetColorResult()));

	// Second appel : EN QUELLE TAILLE. Sans lui, le gel.
	if (!sansTaille) {
		renderer->SetRenderSizeOverride(largeurCible, hauteurCible);
	}

	uint64 image = 0;
	bool tourne = true;

	gDebutImage.store(MillisecondesMaintenant());
	std::thread surveillant(Surveiller, sortie);

	while (tourne && fenetre.IsOpen()) {
		NkEvent *ev = nullptr;
		while (NkEvents().PollEvent(ev)) {
			if (ev->As<NkWindowCloseEvent>() != nullptr) {
				tourne = false;
			}
		}
		if (!tourne) break;

		gImageEnCours.store(image + 1);
		gDebutImage.store(MillisecondesMaintenant());

		if (rEcran->BeginFrame()) {
			NkICommandBuffer *cmd = rEcran->GetCmd();
			NkRender3D *r3d = renderer->GetRender3D();

			renderer->FlushGraphRebuilds();
			r3d->ResetFrame();

			NkCamera3DData cd;
			cd.position = { 1.6f, 1.2f, 2.4f };
			cd.target = { 0.f, 0.f, 0.f };
			cd.up = { 0.f, 1.f, 0.f };
			cd.fovY = 60.f;
			cd.aspect = (float32)largeurCible / (float32)hauteurCible;
			cd.nearPlane = 0.05f;
			cd.farPlane = 100.f;

			NkSceneContext sctx;
			sctx.camera = NkCamera3D(cd);
			sctx.time = (float32)image / 60.f;
			r3d->SetIBLStrength(0.35f);

			NkLightDesc soleil;
			soleil.type = NkLightType::NK_DIRECTIONAL;
			soleil.direction = { -0.5f, -1.f, -0.35f };
			soleil.color = { 1.f, 0.96f, 0.88f };
			soleil.intensity = 3.f;
			soleil.castShadow = false;
			sctx.lights.PushBack(soleil);

			r3d->BeginScene(sctx);
			SoumettreUnCube(r3d, renderer->GetMeshSystem());

			if (auto *graph = renderer->GetRenderGraph()) {
				graph->Execute(cmd);
			}

			if (auto *r2d = rEcran->GetRender2D()) {
				r2d->DrawImage(cible->GetColorResult(),
							   { 0.f, 0.f, (float32)devInit.width,
								 (float32)devInit.height });
			}

			rEcran->Present();
			rEcran->EndFrame();
		}

		++image;

		// L'image est dessinee : on l'ecrit tout de suite, et on vide le
		// tampon, pour que la ligne existe meme si l'image suivante gele.
		gImageTerminee.store(image);
		Ecrire(sortie, "image : %llu, etat : dessinee\n", (unsigned long long)image);

		if (image >= imageMax) {
			tourne = false;
		}
	}

	gFini.store(true);
	surveillant.join();
	if (sortie != nullptr) {
		std::fclose(sortie);
	}

	NkRenderer::Destroy(renderer);
	NkRenderer::Destroy(rEcran);
	NkDeviceFactory::Destroy(device);
	return 0;
}
