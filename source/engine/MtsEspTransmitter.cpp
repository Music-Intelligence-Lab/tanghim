#include "MtsEspTransmitter.h"
#include "libMTSMaster.h"
#include <cmath>

MtsEspTransmitter::MtsEspTransmitter()
{
    if (MTS_CanRegisterMaster())
    {
        DBG ("MtsEspTransmitter: registering as transmitter");
        MTS_RegisterMaster();
        registered = true;
    }
    else if (MTS_HasIPC())
    {
        // IPC is in use but another master appears registered — this can happen
        // if a previous DAW session crashed without calling MTS_DeregisterMaster().
        // Reinitialize the shared memory and re-register.
        DBG ("MtsEspTransmitter: stale IPC detected, reinitializing MTS-ESP");
        MTS_Reinitialize();
        MTS_RegisterMaster();
        registered = true;
    }
    else
    {
        DBG ("MtsEspTransmitter: another transmitter is active (non-IPC), staying inert");
    }

    if (registered)
    {
        // Clear any stale note filters from a previous session (shared memory persists)
        MTS_ClearNoteFilter();

        // Push 12-EDO as the initial tuning table so receivers start with valid data
        double edoTable[128];
        for (int i = 0; i < 128; ++i)
            edoTable[i] = 440.0 * std::pow (2.0, (i - 69) / 12.0);
        MTS_SetNoteTunings (edoTable);

        DBG ("MtsEspTransmitter: registered, filters cleared, 12-EDO pushed");
    }
}

MtsEspTransmitter::~MtsEspTransmitter()
{
    if (registered)
        MTS_DeregisterMaster();
}

bool MtsEspTransmitter::hasExistingTransmitter()
{
    return ! MTS_CanRegisterMaster();
}

int MtsEspTransmitter::numReceivers() const
{
    return registered ? MTS_GetNumClients() : 0;
}

void MtsEspTransmitter::setTuningTable (const std::array<double, 128>& frequenciesHz)
{
    if (! registered) return;
    MTS_SetNoteTunings (frequenciesHz.data());
}

void MtsEspTransmitter::setScaleName (const juce::String& name)
{
    if (! registered) return;
    MTS_SetScaleName (name.toRawUTF8());
}

void MtsEspTransmitter::filterNote (int midiNote, bool shouldFilter)
{
    if (! registered) return;
    // MTS_FilterNote(bool doFilter, char midinote, signed char midichannel)
    // midichannel = -1 means all channels
    MTS_FilterNote (shouldFilter, static_cast<char> (midiNote), -1);
}

void MtsEspTransmitter::clearNoteFilters()
{
    if (! registered) return;
    MTS_ClearNoteFilter();
}
