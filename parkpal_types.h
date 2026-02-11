// parkpal_types.h - Types used across the ParkPal firmware.
//
// This lives in a header (instead of only in the .ino) to avoid Arduino's
// auto-prototype generation issues when functions reference these types.

#pragma once

#include <Arduino.h>
#include <vector>

struct CountdownItem {
    String id;
    String label[4];
    int year = 0;
    int month = 0;
    int day = 0;
    String repeat = "yearly"; // "yearly" | "once"
    int birth_year = 0;
    String accent = "auto"; // "auto" | "red" | "black"
    bool include_in_cycle = true;
    String icon = "auto"; // "auto" | "tree" | "reindeer" | "pumpkin" | "ghost" | "cake" | "none"
};

struct CountdownSettings {
    String show_mode = "single"; // "single" | "cycle"
    String primary_id = "";
    int cycle_every_n_refreshes = 1;
};

struct RuntimeConfig {
    String mode = "parks";
    String resort = "orlando"; // "orlando" | "california" | "tokyo"
    String parks_tz = "EST5EDT,M3.2.0/2,M11.1.0/2";
    String countdowns_tz = "EST5EDT,M3.2.0/2,M11.1.0/2";
    CountdownSettings countdownSettings;
    std::vector<CountdownItem> countdowns;
    bool metric = true;
    bool trip_enabled = true;
    String trip_date = "2026-12-25";
    String trip_name = "";
    int parks[4];
    int parks_n = 0;
    int rideIds[4][6];
    String rideLabels[4][6];
    String legacyNames[4][6];
};

enum IconKind { ICON_NONE, ICON_TREE, ICON_REINDEER, ICON_PUMPKIN, ICON_GHOST, ICON_CAKE };

