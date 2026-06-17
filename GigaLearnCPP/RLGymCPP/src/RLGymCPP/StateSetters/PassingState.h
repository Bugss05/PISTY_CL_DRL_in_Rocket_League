#pragma once
#include "StateSetter.h"

namespace RLGC {
	class PassingState : public StateSetter {
	public:
		void ResetArena(Arena* arena) override;
	};
}
