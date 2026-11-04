// =============================================================================
//  Chapitre 7, exercice 7 — la latence injectée, et la borne.
//
//  Au démarrage, avant d'ouvrir la fenêtre : une tête simulée dont les poses arrivent en
//  retard, comme dans le simulateur du moteur.
//  - Un quart de tour en 400 ms (départ et arrivée doux), commencé à t = 1 s ; une pose par
//    période de 72 hertz pendant trois secondes ; chaque pose est
//    NkQuatf::RotateY(NkAngle(lacet)), sa vitesse vient de la pose précédente.
//  - Le simulateur garde les 64 dernières poses dans un tableau circulaire, chacune avec son
//    instant et sa vitesse. SampleAt() est celui du moteur : avec une latence, il ne rend pas
//    l'échantillon le plus récent mais celui d'il y a « latence » secondes (le plus vieux
//    disponible si l'historique ne remonte pas si loin).
//  - PredictAt() part de cet échantillon et extrapole jusqu'à l'instant d'affichage ; la
//    fenêtre d'extrapolation est bornée à 100 ms, sauf quand on retire la borne.
//  - À chaque instant t (de 1 s à 1,9 s), on prend l'échantillon de la latence, on prédit la
//    pose de t + période (l'instant où l'image sera vue) et on mesure l'angle, en degrés, avec la
//    vraie pose de cet instant, avec la prédiction, puis sans (la pose de l'échantillon telle
//    qu'elle est). Latences : 0, 10, 20, 40 et 80 ms. Une ligne par latence.
//  - Puis la borne : latence d'une seconde, image dont l'échantillon est le plus proche du
//    milieu du geste (t = 1,2 s) ; sans borne, puis avec : instant de l'échantillon, horizon,
//    lacet prédit et vrai (le lacet d'une rotation autour de l'axe vertical est
//    2 * atan2(q.y, q.w)).
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

#include "NKMath/NkQuat.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace nkentseu;
using namespace nkentseu::renderer;

static void ConfigureAppData(NkAppData &d) { d.appName = "Ch07Exo7"; }
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

	// ── La simulation ─────────────────────────────────────────────────────────
	const double kPi = 3.14159265358979323846;
	const double kHz = 72.0;				  // la cadence d'un casque
	const double kPeriode = 1.0 / kHz;		  // une pose par période
	const double kDebutDuGeste = 1.0;		  // le quart de tour commence à t = 1 s
	const double kDureeTotale = 3.0;		  // trois secondes de poses
	const float32 kBorne = 0.1f;			  // la fenêtre d'extrapolation : 100 ms
	const uint32 kMaxSamples = 64;			  // les 64 dernières poses

	// Un quart de tour en 400 ms, départ et arrivée doux. t en secondes depuis le début
	// du geste, lacet en degrés.
	double Lacet(double t) {
		const double amplitude = 90.0, duree = 0.400;
		if (t <= 0.0) return 0.0;
		if (t >= duree) return amplitude;
		return amplitude * 0.5 * (1.0 - std::cos(kPi * t / duree));
	}

	// La vraie pose de la tête à l'instant t (secondes depuis le démarrage de la simulation).
	NkQuatf PoseVraie(double t) {
		return NkQuatf::RotateY(NkAngle((float32)Lacet(t - kDebutDuGeste)));
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

	// L'angle, en degrés, entre deux orientations : 2 * atan2(|(x,y,z)|, |w|) du quaternion
	// (b * a.Conjugate()).Normalized(). C'est le même angle que Angle().Deg(), mais Angle()
	// vaut 2 * acos(|w|) en float32 et ne distingue pas les angles de moins de 0,04 degré.
	double AngleEntre(const NkQuatf &a, const NkQuatf &b) {
		const NkQuatf d = (b * a.Conjugate()).Normalized();
		const double vx = (double)d.x, vy = (double)d.y, vz = (double)d.z;
		const double sinus = std::sqrt(vx * vx + vy * vy + vz * vz);
		return 2.0 * std::atan2(sinus, std::fabs((double)d.w)) * 180.0 / kPi;
	}

	// Le lacet, en degrés, d'une rotation autour de l'axe vertical : 2 * atan2(q.y, q.w).
	double LacetDe(const NkQuatf &q) {
		return 2.0 * std::atan2((double)q.y, (double)q.w) * 180.0 / kPi;
	}

	// ── Le simulateur : un tableau circulaire de 64 poses ─────────────────────
	struct PoseDatee {
		double time;		// l'instant de la pose, en secondes
		NkQuatf pose;
		NkVec3f velocity;	// sa vitesse angulaire (rad/s), tirée de la pose précédente
	};

	class Simulateur {
	public:
		explicit Simulateur(double latenceSecondes) : mLatencySeconds(latenceSecondes) {}

		// Une nouvelle pose, à l'instant t : elle prend la place de la plus ancienne si l'historique
		// est plein ; sa vitesse vient de la pose précédente.
		void Ajouter(double t, const NkQuatf &pose) {
			NkVec3f vitesse(0.f, 0.f, 0.f);
			if (mSampleCount > 0u) {
				const PoseDatee &avant = mSamples[mSampleHead];
				vitesse = VitesseAngulaire(avant.pose, pose, (float32)(t - avant.time));
			}
			mSampleHead = (mSampleHead + 1u) % kMaxSamples;
			mSamples[mSampleHead] = { t, pose, vitesse };
			if (mSampleCount < kMaxSamples) {
				++mSampleCount;
			}
		}

		// Celui du moteur (les instants sont en secondes ici, en nanosecondes là-bas).
		const PoseDatee &SampleAt(double now) const {
			// Latence nulle : l'échantillon le plus récent, point.
			if (mLatencySeconds <= 0.0 || mSampleCount <= 1u) {
				return mSamples[mSampleHead];
			}
			// Latence simulée : on rend l'échantillon d'il y a « latence »
			// secondes — la prédiction doit alors combler l'écart, et c'est
			// exactement ce qu'un test mesure.
			const double target = now - mLatencySeconds;
			uint32 index = mSampleHead;
			for (uint32 i = 0u; i < mSampleCount; ++i) {
				if (mSamples[index].time <= target) {
					return mSamples[index];
				}
				index = (index + kMaxSamples - 1u) % kMaxSamples;
			}
			// Plus vieux disponible : l'historique ne remonte pas si loin.
			return mSamples[(mSampleHead + kMaxSamples - (mSampleCount - 1u)) % kMaxSamples];
		}

		uint32 Compte() const { return mSampleCount; }

	private:
		double mLatencySeconds;
		PoseDatee mSamples[kMaxSamples];
		uint32 mSampleHead = kMaxSamples - 1u;
		uint32 mSampleCount = 0u;
	};

	// L'horizon d'une prédiction : du moment de l'échantillon à l'instant demandé, en secondes.
	// Borné à plus ou moins 100 ms, sauf quand on retire la borne.
	float32 Horizon(const PoseDatee &echantillon, double quand, bool borne) {
		float32 dt = (float32)(quand - echantillon.time);
		if (borne) {
			dt = dt < -kBorne ? -kBorne : (dt > kBorne ? kBorne : dt);
		}
		return dt;
	}

	// La pose prédite à l'instant demandé : on part de l'échantillon et on extrapole.
	NkQuatf PredictAt(const PoseDatee &echantillon, double quand, bool borne) {
		return Extrapoler(echantillon.pose, echantillon.velocity, Horizon(echantillon, quand, borne));
	}

	// Une latence : l'erreur moyenne et la plus grande, de t = 1 s à t = 1,9 s, avec puis sans prédiction.
	void MesurerUneLatence(double latenceMs) {
		Simulateur sim(latenceMs * 1e-3);
		double sommeAvec = 0.0, sommeSans = 0.0, maxAvec = 0.0, maxSans = 0.0;
		size_t n = 0;
		for (size_t k = 0; (double)k * kPeriode <= kDureeTotale + 1e-9; ++k) {
			const double t = (double)k * kPeriode;
			sim.Ajouter(t, PoseVraie(t));
			if (t < 1.0 - 1e-9 || t > 1.9 + 1e-9) {
				continue;
			}
			const PoseDatee &echantillon = sim.SampleAt(t);
			// Ce qui sera vu à t + période : on compare à la vraie pose de CET instant.
			const double vu = t + kPeriode;
			const NkQuatf vraie = PoseVraie(vu);
			const double errAvec = AngleEntre(PredictAt(echantillon, vu, true), vraie);
			const double errSans = AngleEntre(echantillon.pose, vraie);
			sommeAvec += errAvec;
			sommeSans += errSans;
			if (errAvec > maxAvec) maxAvec = errAvec;
			if (errSans > maxSans) maxSans = errSans;
			++n;
		}
		std::printf("latence %g ms : avec %.4f deg (max %.4f), sans %.4f deg (max %.4f)\n",
					latenceMs, sommeAvec / (double)n, maxAvec, sommeSans / (double)n, maxSans);
	}

	// La borne : une latence d'une seconde, l'image dont l'échantillon est le plus proche du
	// milieu du geste (t = 1,2 s) ; sans la borne, puis avec.
	void MesurerLaBorne() {
		const double latence = 1.0;
		Simulateur sim(latence);
		double meilleurT = 0.0;
		double meilleurEcart = 1e30;
		PoseDatee choisi = {};
		for (size_t k = 0; (double)k * kPeriode <= kDureeTotale + 1e-9; ++k) {
			const double t = (double)k * kPeriode;
			sim.Ajouter(t, PoseVraie(t));
			const PoseDatee &echantillon = sim.SampleAt(t);
			const double ecart = std::fabs(echantillon.time - 1.2);
			if (ecart < meilleurEcart) {
				meilleurEcart = ecart;
				meilleurT = t;
				choisi = echantillon;
			}
		}
		const double vu = meilleurT + kPeriode; // l'instant d'affichage de cette image
		const NkQuatf vraie = PoseVraie(vu);
		const bool bornes[2] = { false, true };
		for (int i = 0; i < 2; ++i) {
			const bool borne = bornes[i];
			const NkQuatf predite = PredictAt(choisi, vu, borne);
			std::printf("%s : echantillon a %.4f s, horizon %.3f ms, predit %.3f deg, vrai %.3f deg\n",
						borne ? "avec borne" : "sans borne", choisi.time,
						(double)Horizon(choisi, vu, borne) * 1000.0, LacetDe(predite), LacetDe(vraie));
			std::printf("detail %s : erreur d'angle %.3f deg\n", borne ? "avec borne" : "sans borne",
						AngleEntre(predite, vraie));
		}
		std::printf("detail : image a t = %.4f s, vue a %.4f s, vitesse de l'echantillon %.1f deg/s, "
					"%u poses x %.3f ms = %.3f ms (historique : %.3f ms)\n",
					meilleurT, vu, (double)choisi.velocity.Len() * 180.0 / kPi, (unsigned)kMaxSamples,
					kPeriode * 1000.0, (double)kMaxSamples * kPeriode * 1000.0,
					(double)(kMaxSamples - 1u) * kPeriode * 1000.0);
	}

	// Cinq latences, puis la borne.
	void SimulerLesLatences() {
		const double latencesMs[5] = { 0.0, 10.0, 20.0, 40.0, 80.0 };
		for (int i = 0; i < 5; ++i) {
			MesurerUneLatence(latencesMs[i]);
		}
		MesurerLaBorne();
	}

} // namespace

int nkmain(const NkEntryState &state) {
	(void)state;

	// Les latences : au démarrage, avant d'ouvrir la fenêtre.
	SimulerLesLatences();

	NkWindowConfig cfg;
	cfg.title = "Ch07 Exo7 - la latence et la borne";
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
