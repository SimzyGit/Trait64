# Sécurité

Seule la dernière version publiée reçoit des correctifs.

Pour signaler une faille, utilisez le [signalement privé de GitHub](https://github.com/SimzyGit/Trait64/security/advisories/new), pas une demande publique. Décrivez la version, le système et les étapes pour reproduire. Vous recevrez une réponse sous sept jours.

Points sensibles connus : Trait64 lance les moteurs UCI que vous choisissez avec vos droits, télécharge moteurs, bases et mises à jour en vérifiant leur empreinte SHA-256, et lit des fichiers PGN, EPD et `.t64` potentiellement hostiles (limites de taille et de profondeur décrites dans le README).

*Security reports in English are welcome.*
