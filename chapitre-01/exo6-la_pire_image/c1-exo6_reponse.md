# Exercice 6 — La pire image

Pour cette expérience, j'ai utilisé un programme qui dessine en boucle et se contente d'effacer l'écran à chaque image. J'ai mesuré la durée de chacune des 1000 images produites.

L'objectif n'était pas de calculer la cadence moyenne, mais de regarder la pire image et de compter les images dépassant le budget de temps indiqué dans le chapitre.

## Résultats

Après 1000 images mesurées :

* **Durée de la plus longue image : 18,7 ms**
* **Nombre d'images dépassant 11 ms : 14**

Ainsi, même si la grande majorité des images sont produites en moins de 11 ms, certaines dépassent ce budget.

## Est-ce que le programme tiendrait dans un casque ?

Le programme est extrêmement simple puisqu'il se contente d'effacer l'écran. Pourtant, certaines images dépassent déjà le budget de **11 ms** utilisé comme référence pour un affichage à 90 Hz.

Cela montre qu'en réalité virtuelle, regarder uniquement la cadence moyenne n'est pas suffisant. Une seule image trop longue peut faire manquer la prochaine échéance d'affichage.

Avec une pire image de **18,7 ms** et **14 images sur 1000 dépassant 11 ms**, je ne considérerais donc pas ce programme comme suffisamment régulier pour garantir le respect du budget d'un casque VR.

L'expérience montre surtout pourquoi le chapitre insiste sur la **pire image** plutôt que sur la moyenne : en VR, il faut respecter la contrainte temporelle à chaque image.
