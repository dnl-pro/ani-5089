# Exercice 8 — Le champ de vision asymétrique

J'ai pris comme exemple le **Oculus/Meta Quest 2**.

Pour l'œil gauche, les quatre angles du champ de vision sont :

| Direction |    Angle |
| --------- | -------: |
| Gauche    | **−52°** |
| Droite    | **+45°** |
| Haut      | **+48°** |
| Bas       | **−50°** |

Le champ horizontal total est donc :

**45 − (−52) = 97°**

et le champ vertical total est :

**48 − (−50) = 98°**

Le champ de vision de l'œil gauche est donc légèrement décalé par rapport au centre : il couvre **52° vers la gauche contre 45° vers la droite**, et **50° vers le bas contre 48° vers le haut**.

**Source :** HMD Geometry Database, *Oculus Quest 2 (72Hz)*, données de géométrie du casque. La page indique explicitement pour l'œil gauche : left −52°, right 45°, bottom −50° et top 48°.

Cette asymétrie est cohérente avec la manière dont OpenXR représente le champ de vision : les quatre angles sont définis indépendamment et peuvent donc être différents selon chaque direction.

### Si le champ était symétrique

À surface angulaire équivalente, un champ symétrique redistribuerait cette couverture autour de l'axe central : on perdrait une partie de la couverture périphérique réellement prévue par l'optique du casque, au lieu de conserver exactement la géométrie asymétrique adaptée à l'œil.

Meta explique d'ailleurs que l'utilisation d'un FOV asymétrique permet d'adapter le centre de projection à la position réelle des lentilles et d'éviter de rendre inutilement des pixels qui ne sont pas visibles.
