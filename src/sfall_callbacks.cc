#include "sfall_callbacks.h"

#include "display_monitor.h"
#include "sfall_config.h"
#include "worldmap.h"

namespace fallout {

void sfallOnBeforeGameInit()
{
    return;
}

void sfallOnGameInit()
{
    return;
}

void sfallOnAfterGameInit()
{
    return;
}

void sfallOnGameExit()
{
    return;
}

void sfallOnGameReset()
{
    return;
}

void sfallOnBeforeGameStart()
{
    return;
}

void sfallOnAfterGameStarted()
{
    // Disable Horrigan Patch
    bool isDisableHorrigan = false;
    configGetBool(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_DISABLE_HORRIGAN, &isDisableHorrigan);

    if (isDisableHorrigan) {
        gDidMeetFrankHorrigan = true;
        displayMonitorAddMessage(SFALL_CONFIG_DISABLE_HORRIGAN);
    }
}

void sfallOnAfterNewGame()
{
    return;
}

void sfallOnGameModeChange()
{
    return;
}

void sfallOnBeforeGameClose()
{
    return;
}

void sfallOnCombatStart()
{
    return;
}

void sfallOnCombatEnd()
{
    return;
}

void sfallOnBeforeMapLoad()
{
    return;
}

} // namespace fallout
