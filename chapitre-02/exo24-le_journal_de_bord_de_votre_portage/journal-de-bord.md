## Entrée 1

Symptôme : `wsl --install` échoue avec un message « Accès refusé » dans PowerShell.

J'ai cru : que WSL était mal installé ou que mon ordinateur ne supportait pas WSL 2.

C'était : une commande qui nécessitait des privilèges administrateur et une configuration correcte de Windows.

Temps perdu : 20 minutes

## Entrée 2

Symptôme : la compilation C++ échoue parce que `clang++` n'est pas reconnu comme commande.

J'ai cru : que mon programme C++ contenait une erreur ou que Visual Studio devait obligatoirement être installé pour compiler.

C'était : le compilateur Clang qui n'était pas encore installé et configuré dans mon environnement Linux.

Temps perdu : 30 minutes

## Entrée 3

Symptôme : Python refuse certains paquets et des erreurs apparaissent avec des bibliothèques lors de l'installation de l'environnement.

J'ai cru : que le problème venait uniquement de mon code Python.

C'était : une incompatibilité entre la version de Python utilisée et certaines versions des bibliothèques.

Temps perdu : 25 minutes

## Entrée 4

Symptôme : un test d'exercice affiche une sortie vide alors que le programme compile correctement.

J'ai cru : que l'algorithme de recherche du symbole ne fonctionnait pas.

C'était : le format de l'entrée de test qui ne contenait pas la chaîne exacte recherchée par le programme, notamment le marqueur `undefined reference to '`.

Temps perdu : 15 minutes

## Entrée 5

Symptôme : le premier `git push` est refusé parce que le dépôt distant contient déjà des modifications.

J'ai cru : que mon dépôt local était complètement synchronisé avec GitHub puisque mes exercices étaient présents localement.

C'était : un historique différent entre le dépôt local et le dépôt distant. Il fallait récupérer les modifications distantes avec `git pull --rebase` avant de pouvoir pousser.

Temps perdu : 20 minutes

