#pragma once
#include "StateSetter.h"

namespace RLGC {
	class KickoffState : public StateSetter {
	public:
		void ResetArena(Arena* arena) {
			arena->ResetToRandomKickoff();
			// INFINITE BOOST OVERRIDE
			for (Car* car : arena->_cars) {
				CarState cs = car->GetState();
				cs.boost = 100.0f;
				car->SetState(cs);
			}
		}
	};
}