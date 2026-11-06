#ifndef CHAINE_H
#define CHAINE_H

#include <cstdint>

// Chaine d'echange : un anneau de poignees, un drapeau d'emprunt et deux
// methodes qui refusent au lieu de planter (elles renvoient false).
// Le maximum est petit et connu, donc aucun tableau dynamique.
class ChaineEchange {
public:
    static constexpr std::uint32_t MaxImages = 4;

    // Refuse une chaine vide ou plus grande que le maximum.
    bool Creer(std::uint32_t nombre) {
        if (nombre == 0 || nombre > MaxImages) {
            return false;
        }
        mNombre = nombre;
        mSuivant = 0;
        mEmprunte = false;
        for (std::uint32_t i = 0; i < MaxImages; ++i) {
            mPoignees[i] = 0;
        }
        return true;
    }

    // Refus du double emprunt : une image est deja sortie.
    bool Emprunter(std::uint32_t& index) {
        if (mNombre == 0 || mEmprunte) {
            return false;
        }
        mEmprunte = true;
        index = mSuivant;
        return true;
    }

    // Refus de rendre sans avoir emprunte. Rendre fait avancer l'anneau.
    bool Rendre(std::uint32_t& index) {
        if (mNombre == 0 || !mEmprunte) {
            return false;
        }
        index = mSuivant;
        mEmprunte = false;
        mSuivant = (mSuivant + 1) % mNombre;
        return true;
    }

    // Derniere image rendue : on recule d'un cran sur l'anneau.
    std::uint32_t DerniereRendue() const {
        return (mSuivant + mNombre - 1) % mNombre;
    }

    // La chaine garde des entiers sans type et ne les interprete jamais.
    void Enregistrer(std::uint32_t index, std::uint64_t poignee) {
        if (index < mNombre) {
            mPoignees[index] = poignee;
        }
    }

    std::uint64_t Poignee(std::uint32_t index) const {
        return index < mNombre ? mPoignees[index] : 0;
    }

    bool EstEmprunte() const { return mEmprunte; }
    std::uint32_t Nombre() const { return mNombre; }

private:
    std::uint64_t mPoignees[MaxImages] = {0, 0, 0, 0};
    std::uint32_t mNombre = 0;
    std::uint32_t mSuivant = 0;
    bool mEmprunte = false;
};

#endif
