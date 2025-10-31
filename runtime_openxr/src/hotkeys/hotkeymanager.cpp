/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "hotkeymanager.h"
#include <algorithm>

#ifdef WIN32
#include "hotkeywindows.h"
#endif

HotkeyManager::HotkeyManager(EventManager& event_manager) {
#ifdef WIN32
    implementation = std::make_shared<WindowsHotkeyImplementation>();
#endif

    event_stream_writer = event_manager.CreateEventStream(EventStreamType::GB_EVENT_STREAM_TYPE_HOTKEY, 100, 0);
}

// Polls the hotkeys using the current implementation of the IHotkeys interface. (Currently defaults to the Windows Implementation).
bool HotkeyManager::PollHotkeys() {
    //Check if hotkey implementation is initialized
    if (this->implementation == nullptr){
        //Hotkey Implementation is not initialized.
        return false;
    }

    // Adds all registered hotkeys to the list of hotkeys.
    std::vector<IHotkeys::CombinedHotkeyStrokes> hotkeys;
    for(auto it = registered_hotkeys.begin(); it != registered_hotkeys.end(); it++) {
        hotkeys.push_back(it->hotkey_combination);
    }

    //Let the IHotkeys interface check the hotkeys, they can be retrieved later from its "hotkey_states" member.
    if(this->implementation->CheckHotkeys(hotkeys).empty()){
        // No hotkeys were registered
        return false;
    }

    return true;
}

void HotkeyManager::AddHotkey(GameBridgeHotKeyEvent event_type, uint8_t first_keystroke, uint8_t second_keystroke, uint8_t third_keystroke, uint8_t fourth_keystroke) {
    // Start by combining all shortcuts into the union.
    IHotkeys::CombinedHotkeyStrokes received_strokes{};
    received_strokes.separatedStrokes[0] = first_keystroke;
    received_strokes.separatedStrokes[1] = second_keystroke;
    received_strokes.separatedStrokes[2] = third_keystroke;
    received_strokes.separatedStrokes[3] = fourth_keystroke;

    // Sort all keystrokes
    std::sort(&received_strokes.separatedStrokes[0],&received_strokes.separatedStrokes[4]);

    HotkeyContainer tempContainer;
    tempContainer.hotkey_combination = received_strokes;
    tempContainer.hotkey_event = event_type;
    registered_hotkeys.push_back(tempContainer);
}

void HotkeyManager::RemoveHotkey(CombinedStrokes combined_number, GameBridgeHotKeyEvent event_type) {
    for (auto it = registered_hotkeys.begin(); it != registered_hotkeys.end(); ++it) {
        if (it->hotkey_combination.combinedNumber == combined_number && it->hotkey_event == event_type) {
            // Found matching hotkey, time to remove it!
            registered_hotkeys.erase(it);
            return;
        }
    }
}

bool HotkeyManager::SendHotkeyEvents() {
    for (auto it = registered_hotkeys.begin(); it != registered_hotkeys.end(); ++it) {
        // Check if the hotkey state of the current hotkey combination is true.
        if (implementation->hotkey_states[it->hotkey_combination]) {
            // Found pressed hotkey, send event.
            if(!event_stream_writer->SubmitEvent(it->hotkey_event, 0, nullptr)) {
                return false;
            }
        }
    }
    return true;
}
