# Exercice 7 — Le coût du doublement

J'ai repris le programme de l'exercice précédent et me suis intéressé uniquement au temps nécessaire pour effectuer son rendu, sans prendre en compte la logique de l'application.

## Résultats

Le temps de rendu d'une image est d'environ **2,4 ms**.

Si le même rendu devait être effectué deux fois, par exemple pour produire une image différente pour chacun des deux yeux, on peut estimer le coût à :

**2 × 2,4 = 4,8 ms**

À 90 Hz, une image doit être produite dans un budget d'environ **11,1 ms**.

Il resterait donc :

**11,1 − 4,8 = 6,3 ms**

pour toutes les autres opérations de l'application.

| Mesure                      |  Valeur |
| --------------------------- | ------: |
| Rendu d'une image           |  2,4 ms |
| Rendu deux fois estimé      |  4,8 ms |
| Budget à 90 Hz              | 11,1 ms |
| Temps restant pour le reste |  6,3 ms |

## Conclusion

Le doublement du rendu consomme une partie importante du budget disponible, mais il ne suffit pas à lui seul à dépasser le budget de 11,1 ms dans cet exemple.

La priorité serait donc de réduire le coût du rendu s'il devient plus complexe, mais aussi de surveiller les autres opérations qui utilisent les **6,3 ms restantes**. En réalité virtuelle, il faut prendre en compte l'ensemble de la chaîne et pas seulement le temps de rendu.

Cette expérience montre également qu'une optimisation utile doit être recherchée là où le temps est réellement consommé : réduire une opération déjà très rapide aura peu d'effet si une autre partie du programme constitue le principal coût.
