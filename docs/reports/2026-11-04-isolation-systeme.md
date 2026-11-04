# Verification locale de l'isolation systeme

## Objet

Verifier si un montage de workspace avec bubblewrap suffit a empecher la
lecture d'un fichier exterieur qui possede un lien physique dans le workspace.

## Essai

Sur Linux, `bwrap --unshare-user` a demarre. Deux fichiers temporaires ont ete
crees : un fichier hors du repertoire workspace et un lien physique vers ce
fichier dans le workspace. Dans un environnement bubblewrap ou seul le
workspace etait monte sous `/workspace`, la lecture de
`/workspace/shared.txt` a retourne le marqueur du fichier exterieur.

## Conclusion et limites

Un montage isole du workspace ne suffit pas a interdire l'acces au contenu des
liens physiques qui y sont deja presents. Cet essai ne teste ni les courses
sur les noms temporaires ni les deplacements concurrents de repertoires ; il
ne constitue pas une evaluation complete de bubblewrap ou d'autres solutions.
Aucune isolation systeme n'a ete ajoutee a l'application.
