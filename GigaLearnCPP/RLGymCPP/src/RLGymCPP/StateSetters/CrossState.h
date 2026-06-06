#pragma once
#include "StateSetter.h"

namespace RLGC {
	class CrossState : public StateSetter {
	public:
		virtual void ResetArena(Arena* arena) override;
	};
}
