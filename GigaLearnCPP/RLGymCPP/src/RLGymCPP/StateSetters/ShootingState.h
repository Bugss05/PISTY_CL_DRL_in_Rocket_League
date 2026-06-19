#pragma once
#include "StateSetter.h"

namespace RLGC {
    class ShootingState : public StateSetter {
    public:
        // Empty constructor so the compiler doesn't complain
        ShootingState() {}

        virtual void SetParam(const std::string& key, float value) override {
            // Empty for now
        }

        virtual void ResetArena(Arena* arena) override;
    };
}