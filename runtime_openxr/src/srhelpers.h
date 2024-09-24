#pragma once
#include <unordered_map>

#include <game_bridge.h>

#include "openxr_includes.h"

namespace SR {
    class SRContext;
}

// TODO SR display class to toggle the lens on the screen etc...
namespace XRGameBridge {
    /*
    * SRInstance class
    * Takes care of the connection to the sr service and is aware of connected SR displays.
    */
    class GameBridgeInstance {
        //std::unordered_map<XrSRContext, SR::Screen*> sr_screens;
        //std::unordered_map<XrSRContext, SR::SwitchableLensHint*> sr_lenhints;



    public:
        GameBridgeInstance();
        ~GameBridgeInstance();
        SR::SRContext* GetSrContext();
        void GetScreen();
    };
}
