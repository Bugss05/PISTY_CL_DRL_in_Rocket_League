#pragma once
#include "StateSetter.h"

namespace RLGC {
	class PassState : public StateSetter {
	public:
		virtual void ResetArena(Arena* arena) override;
	};
}
