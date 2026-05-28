#!/usr/bin/env python3
"""Patch demo.ino to add WiFi on/off toggle in settings menu."""

def main():
    path = 'examples/demo/demo.ino'
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 1. Add global WiFi enabled variable after lastWifiConnected
    content = content.replace(
        'bool lastWifiConnected = false;',
        'bool lastWifiConnected = false;\nbool wifiEnabled = true;'
    )

    # 2. Add WiFi toggle function declaration after drawSettingsMenuScreen
    content = content.replace(
        'static void drawSettingsMenuScreen();',
        'static void drawSettingsMenuScreen();\nstatic void toggleWifi();'
    )

    # 3. Modify drawSettingsMenuScreen to add WiFi row
    old_menu = '''    // Menu Item 3: Volume
    int item3Y = 418;
    portraitFillRect(34, item3Y, 472, 90, 0xFF);
    portraitDrawRect(34, item3Y, 472, 90, 0x00);
    portraitDrawRect(36, item3Y + 2, 468, 86, 0x00);
    drawPortraitTextInRectCentered("Volume", 34, item3Y, 340, 90, (GFXfont *)&FiraSans);
    drawPortraitTextInRectCentered(">", 370, item3Y, 80, 90, (GFXfont *)&FiraSans);
}

static bool ensureSdReady()'''
    
    new_menu = '''    // Menu Item 3: WiFi Toggle
    int item3Y = 418;
    portraitFillRect(34, item3Y, 472, 90, 0xFF);
    portraitDrawRect(34, item3Y, 472, 90, 0x00);
    portraitDrawRect(36, item3Y + 2, 468, 86, 0x00);
    drawPortraitTextInRectCentered("WiFi", 34, item3Y, 280, 90, (GFXfont *)&FiraSans);
    const char *wifiStatus = wifiEnabled ? "ON" : "OFF";
    drawPortraitTextInRectCentered(wifiStatus, 310, item3Y, 100, 90, (GFXfont *)&FiraSans);
}

static void toggleWifi()
{
    wifiEnabled = !wifiEnabled;
    if (wifiEnabled) {
        // Turn WiFi on
        WiFi.mode(WIFI_STA);
        if (saved_wifi_ssid[0] != '\\0') {
            WiFi.begin(saved_wifi_ssid, saved_wifi_password);
        } else {
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
    } else {
        // Turn WiFi off
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    }
}

static bool ensureSdReady()'''
    
    content = content.replace(old_menu, new_menu)

    # 4. Modify handleSettingsMenuTouch to handle WiFi toggle
    old_touch = '''    // Menu Item 3: Volume (Y=418) - placeholder for now
    if (pointInRect(px, py, 34, 418, 472, 90)) {
        // Placeholder: stay on the menu for now
        return true;
    }

    return false;
}

static bool handleContentSettingsTouch'''
    
    new_touch = '''    // Menu Item 3: WiFi Toggle (Y=418)
    if (pointInRect(px, py, 34, 418, 472, 90)) {
        toggleWifi();
        refreshDisplay(drawSettingsMenuScreen);
        return true;
    }

    return false;
}

static bool handleContentSettingsTouch'''
    
    content = content.replace(old_touch, new_touch)

    # 5. Modify WiFi reconnect logic to respect wifiEnabled flag
    content = content.replace(
        'if (!wifiConnected && saved_wifi_ssid[0] != \'\\0\' && millis() > wifi_reconnect_interval) {',
        'if (!wifiConnected && wifiEnabled && saved_wifi_ssid[0] != \'\\0\' && millis() > wifi_reconnect_interval) {'
    )

    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print("WiFi toggle patch applied successfully!")

if __name__ == '__main__':
    main()
