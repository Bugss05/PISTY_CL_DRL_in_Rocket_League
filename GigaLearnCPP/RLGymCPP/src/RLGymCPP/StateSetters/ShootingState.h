#pragma once
#include "StateSetter.h"

namespace RLGC {
    class ShootingState : public StateSetter {
    public:
        // Construtor vazio para o compilador não reclamar
        ShootingState() {}

        virtual void SetParam(const std::string& key, float value) override {
            // Vazio por agora
        }

        virtual void ResetArena(Arena* arena) override;
    };
}