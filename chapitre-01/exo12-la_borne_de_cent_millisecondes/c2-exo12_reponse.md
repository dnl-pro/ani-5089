import numpy as np
import matplotlib.pyplot as plt

vitesse = 180
acceleration = 30
dt = 0.001

durees = np.arange(0.01, 1.01, 0.01)
erreurs = []

for duree in durees:
    angle_reel = 0
    temps = 0

    while temps < duree:
        vitesse_reelle = vitesse + acceleration * temps
        angle_reel += vitesse_reelle * dt
        temps += dt

    angle_prevu = vitesse * duree
    erreurs.append(abs(angle_reel - angle_prevu))

plt.figure(figsize=(8, 5))
plt.plot(durees * 1000, erreurs)
plt.axvline(100, linestyle="--")
plt.xlabel("Durée d'extrapolation (ms)")
plt.ylabel("Erreur angulaire (degrés)")
plt.title("Erreur d'extrapolation d'une pose de tête")
plt.grid(True)
plt.show()
