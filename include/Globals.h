#ifndef GLOBALS_H
#define GLOBALS_H

// Menylägen
enum MenuSelection {
  SEL_ALL = 0,
  SEL_WHITE = 1,
  SEL_RED = 2,
  SEL_UV = 3,
  SEL_PRESETS = 4,   // NEW FEATURE 1
  SEL_CLOCK = 5,
  SEL_SETTINGS = 6,
  SEL_QR = 7,
  SEL_INFO = 8
};

// Inställningsmenyns val
enum SettingOption {
  SET_LANG = 0,
  SET_ECO = 1,
  SET_QR = 2,
  SET_INFO = 3,
  SET_REBOOT = 4
};

// Språkhantering
enum Language { LANG_SV = 0, LANG_EN = 1 };

// Plant Presets (New Feature 2 context)
enum PlantPreset {
    PRESET_NONE = 0,
    PRESET_SEED,  // Low light, White dominant
    PRESET_VEG,   // High White, Medium Red
    PRESET_BLOOM, // High Red, Medium White, Low UV
    PRESET_FULL   // Max All
};

#endif
