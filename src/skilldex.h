#ifndef SKILLDEX_H
#define SKILLDEX_H

namespace fallout {

typedef enum SkilldexRC {
    SKILLDEX_RC_ERROR = -1,
    SKILLDEX_RC_CANCELED,
    SKILLDEX_RC_SNEAK,
    SKILLDEX_RC_LOCKPICK,
    SKILLDEX_RC_STEAL,
    SKILLDEX_RC_TRAPS,
    SKILLDEX_RC_FIRST_AID,
    SKILLDEX_RC_DOCTOR,
    SKILLDEX_RC_SCIENCE,
    SKILLDEX_RC_REPAIR,
} SkilldexRC;

int skilldexOpen();

} // namespace fallout

#endif /* SKILLDEX_H */
