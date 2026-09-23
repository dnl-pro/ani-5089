# Exercice 12 — Vingt millisecondes senties

J'ai réalisé un programme permettant de faire suivre un point à la position de la souris avec un retard réglable de 0 à 200 ms.

J'ai ensuite fait varier progressivement le retard et demandé à cinq personnes d'indiquer à partir de quel moment elles percevaient un décalage entre le mouvement de la souris et celui du point affiché.

## Résultats

| Personne   | Seuil à partir duquel le retard est ressenti |
| ---------- | -------------------------------------------: |
| Personne 1 |                                        20 ms |
| Personne 2 |                                        30 ms |
| Personne 3 |                                        20 ms |
| Personne 4 |                                        40 ms |
| Personne 5 |                                        30 ms |

La moyenne des cinq seuils est :

**(20 + 30 + 20 + 40 + 30) / 5 = 28 ms**

Le seuil le plus faible est de **20 ms**, tandis que le plus élevé est de **40 ms**. L'écart entre les deux est donc de **20 ms**.

## Comparaison avec le budget de 20 ms

Le chapitre donne environ **20 ms** comme ordre de grandeur pour la chaîne « mouvement vers photon ». Dans notre expérience sur écran, plusieurs personnes commencent déjà à remarquer le retard autour de cette valeur, même si le seuil varie d'une personne à l'autre.

Le seuil est particulièrement contraignant en réalité virtuelle parce qu'il ne s'agit pas seulement de suivre un curseur sur un écran. Dans un casque, l'image doit rester synchronisée avec les mouvements de la tête. Lorsque la tête bouge, le système doit rapidement mesurer ce mouvement, calculer la nouvelle image et l'afficher.

Un retard entre le mouvement réel de la tête et le mouvement visuel peut donc créer une différence entre les informations reçues par les yeux et celles provenant du système vestibulaire. C'est cette discordance qui peut rendre une latence particulièrement gênante en réalité virtuelle.

Cette expérience permet donc de comprendre pourquoi le budget de latence d'un casque VR doit être très faible et respecté à chaque image.
