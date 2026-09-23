# Exercice 2 — Le tableau des budgets

Le chapitre présente la latence « mouvement vers photon » comme une succession de cinq étapes. L'objectif de cet exercice est de reprendre ces cinq étapes et de rechercher, pour chacune d'elles, une valeur mesurée ou documentée dans une source externe.

Toutes les valeurs ne sont pas nécessairement disponibles de manière isolée. Lorsque je n'ai pas trouvé de mesure correspondant précisément à une étape, je le précise plutôt que d'attribuer une valeur approximative.

| Étape                               | Ordre de grandeur du chapitre | Valeur mesurée ou documentée                                 | Source                                                                                            |
| ----------------------------------- | ----------------------------: | ------------------------------------------------------------ | ------------------------------------------------------------------------------------------------- |
| Les capteurs mesurent le mouvement  |                      1 à 2 ms | Environ 1 ms pour une fréquence d'échantillonnage de 1000 Hz | VR & AR Wiki, *Latency*                                                                           |
| Le système transmet la mesure       |                      1 à 3 ms | **Non trouvée**                                              | Aucune mesure isolée suffisamment fiable trouvée                                                  |
| Votre application décide et dessine |                     5 à 11 ms | 10,72 ms de temps de trame dans un scénario VR               | *Efficient Real-Time Rendering and Optimization Using DirectX12 for Unreal Engine Graphics*, 2026 |
| Le compositeur assemble             |                      1 à 2 ms | **Non trouvée**                                              | Aucune mesure isolée comparable trouvée                                                           |
| L'écran affiche la ligne            |                      2 à 5 ms | Environ 5 à 8 ms selon la position sur le panneau            | Brevet sur la compensation de la latence d'affichage en VR                                        |

## 1. Les capteurs mesurent le mouvement

Une synthèse consacrée à la latence en réalité virtuelle indique qu'un système optimisé peut avoir une latence d'échantillonnage de l'IMU inférieure à 1 ms. De plus, à une fréquence de 1000 Hz, une nouvelle mesure est disponible toutes les 1 ms.

Cette valeur donne donc un ordre de grandeur cohérent avec les 1 à 2 ms indiquées dans le chapitre. Il faut toutefois faire attention à l'interprétation : **1 ms correspond ici à la période d'échantillonnage à 1000 Hz et non à une mesure universelle de la latence complète d'un capteur VR**.

**Source :** VR & AR Wiki, *Latency*.

## 2. Le système transmet la mesure

Pour cette étape, je n'ai pas trouvé de source suffisamment fiable donnant une mesure isolée du temps nécessaire pour transmettre la mesure du capteur jusqu'à l'application.

Les sources consacrées à la latence VR regroupent généralement cette partie avec d'autres opérations, comme le suivi de la position, le traitement des données ou le rendu. Il serait donc difficile d'attribuer une valeur précise de 1, 2 ou 3 ms uniquement à la transmission.

**Valeur retenue : non trouvée.**

## 3. Votre application décide et dessine

Une étude publiée en 2026 sur le rendu temps réel avec DirectX 12 mesure, dans un scénario VR, un temps de trame de **10,72 ms**, contre 13,89 ms dans la configuration comparée.

Cette valeur est intéressante pour cette étape puisqu'elle concerne directement le temps nécessaire à la production d'une trame dans l'application. Elle ne correspond cependant pas exactement aux seules opérations de « décision et dessin » décrites dans le chapitre : le temps de trame englobe plusieurs opérations du pipeline de rendu.

On peut donc utiliser **10,72 ms comme valeur mesurée de référence pour le temps de trame**, mais pas comme une mesure universelle de cette étape.

**Source :** *Efficient Real-Time Rendering and Optimization Using DirectX12 for Unreal Engine Graphics*, 2026.

## 4. Le compositeur assemble

Pour le compositeur, les documentations techniques décrivent bien son rôle dans la chaîne de rendu. Il intervient notamment après le rendu de l'application et peut effectuer des opérations comme la composition, la correction de distorsion et certains mécanismes destinés à réduire la latence.

En revanche, je n'ai pas trouvé de mesure publique permettant d'isoler précisément le temps consacré uniquement au compositeur et de la comparer directement aux **1 à 2 ms** indiquées dans le chapitre.

**Valeur retenue : non trouvée.**

**Source de documentation :** Meta Horizon OS Developers, documentation sur le rendu et le compositeur VR.

## 5. L'écran affiche la ligne

Un brevet consacré à la compensation de la latence variable des dispositifs d'affichage en réalité virtuelle indique, pour un écran à balayage, une latence pouvant être d'environ **5 ms en haut du panneau et 8 ms en bas**.

Cette valeur montre notamment que l'affichage d'une image ne se fait pas simultanément sur toute la surface de l'écran. La position de la ligne affichée dans le panneau peut donc modifier le délai avant que le pixel correspondant ne soit effectivement visible.

Cette valeur est du même ordre de grandeur que les **2 à 5 ms** indiquées dans le chapitre, même si elle ne correspond pas exactement au même dispositif ou aux mêmes conditions expérimentales.

**Source :** brevet, *Techniques for compensating variable display device latency in image display of virtual reality*.

## Vérification avec une mesure de latence mouvement-vers-photon

Pour avoir un point de comparaison avec l'ensemble de la chaîne, une étude publiée dans *Behavior Research Methods* a mesuré directement la latence « mouvement vers photon » de plusieurs systèmes VR, notamment le HTC Vive, l'Oculus Rift, l'Oculus Rift S et le Valve Index.

Lors du début d'un mouvement brusque, les latences moyennes mesurées étaient comprises entre **21 et 42 ms**. Les auteurs montrent également que la prédiction du mouvement peut réduire la latence fonctionnelle à environ **2 à 13 ms** dans les conditions étudiées.

Cette étude ne permet pas de remplacer les cinq mesures demandées dans le tableau, car elle mesure la **latence totale mouvement-vers-photon**. Elle constitue cependant une vérification intéressante : dans un système réel, les différents délais s'additionnent et la prédiction peut modifier la latence effectivement ressentie.

**Source :** Warburton et al., *Measuring motion-to-photon latency for sensorimotor experiments with virtual reality systems*, *Behavior Research Methods*, 2023.

## Conclusion

La recherche montre que toutes les étapes du budget présenté dans le chapitre ne disposent pas d'une mesure publique isolée.

Pour les capteurs, on trouve un ordre de grandeur autour de la milliseconde, tandis qu'un temps de trame de **10,72 ms** a été mesuré dans un scénario VR utilisant DirectX 12. Pour l'affichage, une documentation sous forme de brevet donne un exemple d'environ **5 à 8 ms** selon la position sur le panneau.

En revanche, je n'ai pas trouvé de mesure suffisamment précise et isolée pour les étapes **« transmission de la mesure »** et **« compositeur »**. Ces deux valeurs sont donc volontairement indiquées comme **non trouvées**.

C'est justement l'un des points importants de l'exercice : lorsqu'une mesure fiable n'est pas disponible, il vaut mieux le signaler que fabriquer une valeur pour compléter artificiellement le tableau.
