#pragma once
#include "StateSetter.h"

namespace RLGC {
	class AirShotState : public StateSetter {
	public:
		virtual void ResetArena(Arena* arena) override;
	};
}