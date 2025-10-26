/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

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
