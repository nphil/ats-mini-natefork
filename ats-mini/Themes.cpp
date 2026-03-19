#include "Common.h"
#include "Themes.h"
#include "Draw.h"
#include <math.h>

ColorTheme theme[] =
{
  {
    "Default",
    0x0000, // bg
    0xFFFF, // text
    0xD69A, // text_muted
    0xF800, // text_warn
    0xD69A, // smeter_icon
    0x07E0, // smeter_bar
    0xF800, // smeter_bar_plus
    0x3186, // smeter_bar_empty
    0xF800, // save_icon
    0xD69A, // stereo_icon
    0xF800, // rf_icon
    0x07E0, // rf_icon_conn
    0xFFFF, // batt_voltage
    0xFFFF, // batt_border
    0x07E0, // batt_full
    0xF800, // batt_low
    0x0000, // batt_charge
    0xFFE0, // batt_icon
    0xD69A, // band_text
    0xD69A, // mode_text
    0xD69A, // mode_border
    0x0000, // box_bg
    0xD69A, // box_border
    0xD69A, // box_text
    0xF800, // box_off_bg
    0xBEDF, // box_off_text
    0x0000, // menu_bg
    0xF800, // menu_border
    0xFFFF, // menu_hdr
    0xBEDF, // menu_item
    0x105B, // menu_hl_bg
    0xBEDF, // menu_hl_text
    0xBEDF, // menu_param
    0xFFFF, // freq_text
    0xD69A, // funit_text
    0xF800, // freq_hl
    0xFFE0, // freq_hl_sel
    0xD69A, // rds_text
    0xFFFF, // scale_text
    0xF800, // scale_pointer
    0xC638, // scale_line
    0x94B2, // scan_grid
    0x0659, // scan_snr
    0x07E0, // scan_rssi
  },

  {
    "Bluesky",
    0x2293, // bg
    0xFFFF, // text
    0xD69A, // text_muted
    0xF800, // text_warn
    0xD69A, // smeter_icon
    0x07E0, // smeter_bar
    0xF800, // smeter_bar_plus
    0x3AF3, // smeter_bar_empty
    0xF800, // save_icon
    0xD69A, // stereo_icon
    0xF800, // rf_icon
    0x07E0, // rf_icon_conn
    0xFFFF, // batt_voltage
    0xFFFF, // batt_border
    0x07E0, // batt_full
    0xF800, // batt_low
    0x2293, // batt_charge
    0xFFE0, // batt_icon
    0xD69A, // band_text
    0xD69A, // mode_text
    0xD69A, // mode_border
    0x2293, // box_bg
    0xD69A, // box_border
    0xD69A, // box_text
    0xF800, // box_off_bg
    0xBEDF, // box_off_text
    0x2293, // menu_bg
    0xF800, // menu_border
    0xFFFF, // menu_hdr
    0xBEDF, // menu_item
    0xD69A, // menu_hl_bg
    0x2293, // menu_hl_text
    0xBEDF, // menu_param
    0xFFFF, // freq_text
    0xD69A, // funit_text
    0xF800, // freq_hl
    0xFFE0, // freq_hl_sel
    0xD69A, // rds_text
    0xFFFF, // scale_text
    0xF800, // scale_pointer
    0xC638, // scale_line
    0x94B2, // scan_grid
    0x07FF, // scan_snr
    0x07E0, // scan_rssi
  },

  {
    "eInk",
    0xC616, // bg
    0x3A08, // text
    0x632C, // text_muted
    0xF800, // text_warn
    0x18C3, // smeter_icon
    0x632C, // smeter_bar
    0x18C3, // smeter_bar_plus
    0xB594, // smeter_bar_empty
    0x632C, // save_icon
    0x632C, // stereo_icon
    0x3A08, // rf_icon
    0x632C, // rf_icon_conn
    0x18C3, // batt_voltage
    0x0000, // batt_border
    0x632C, // batt_full
    0x3A08, // batt_low
    0xC616, // batt_charge
    0x3A08, // batt_icon
    0x3A08, // band_text
    0x632C, // mode_text
    0x632C, // mode_border
    0xC616, // box_bg
    0x3A08, // box_border
    0x3A08, // box_text
    0x632C, // box_off_bg
    0xC616, // box_off_text
    0xC616, // menu_bg
    0x3A08, // menu_border
    0x18C3, // menu_hdr
    0x3A08, // menu_item
    0x3A08, // menu_hl_bg
    0xC616, // menu_hl_text
    0x3A08, // menu_param
    0x3A08, // freq_text
    0x632C, // funit_text
    0x0000, // freq_hl
    0x632C, // freq_hl_sel
    0x632C, // rds_text
    0x18C3, // scale_text
    0x0000, // scale_pointer
    0x632C, // scale_line
    0x94B2, // scan_grid
    0x94B2, // scan_snr
    0x18C3, // scan_rssi
  },

  {
    "Pager",
    0x4309, // bg
    0x00C2, // text
    0x1165, // text_muted
    0xF800, // text_warn
    0x18C3, // smeter_icon
    0x1165, // smeter_bar
    0x00C2, // smeter_bar_plus
    0x3287, // smeter_bar_empty
    0x18C3, // save_icon
    0x00C2, // stereo_icon
    0x00C2, // rf_icon
    0x1165, // rf_icon_conn
    0x18C3, // batt_voltage
    0x0000, // batt_border
    0x1165, // batt_full
    0x00C2, // batt_low
    0x4309, // batt_charge
    0x00C2, // batt_icon
    0x00C2, // band_text
    0x00C2, // mode_text
    0x00C2, // mode_border
    0x4309, // box_bg
    0x00C2, // box_border
    0x00C2, // box_text
    0x00C2, // box_off_bg
    0x4309, // box_off_text
    0x4309, // menu_bg
    0x00C2, // menu_border
    0x18C3, // menu_hdr
    0x00C2, // menu_item
    0x00C2, // menu_hl_bg
    0x4309, // menu_hl_text
    0x00C2, // menu_param
    0x00C2, // freq_text
    0x1165, // funit_text
    0x0000, // freq_hl
    0x1165, // freq_hl_sel
    0x1165, // rds_text
    0x18C3, // scale_text
    0x0000, // scale_pointer
    0x1165, // scale_line
    0x18C3, // scan_grid
    0x2A25, // scan_snr
    0x00C2, // scan_rssi
  },

  {
    "Orange",
    0xF3C1, // bg
    0x18C3, // text
    0x2945, // text_muted
    0xF800, // text_warn
    0x18C3, // smeter_icon
    0x4208, // smeter_bar
    0x2945, // smeter_bar_plus
    0xE320, // smeter_bar_empty
    0x4208, // save_icon
    0x2945, // stereo_icon
    0x2945, // rf_icon
    0x4208, // rf_icon_conn
    0x18C3, // batt_voltage
    0x0000, // batt_border
    0x4208, // batt_full
    0x2945, // batt_low
    0xF3C1, // batt_charge
    0x18C3, // batt_icon
    0x18C3, // band_text
    0x2945, // mode_text
    0x2945, // mode_border
    0xF3C1, // box_bg
    0x2945, // box_border
    0x2945, // box_text
    0x2945, // box_off_bg
    0xF3C1, // box_off_text
    0xF3C1, // menu_bg
    0x18C3, // menu_border
    0x18C3, // menu_hdr
    0x2945, // menu_item
    0x2945, // menu_hl_bg
    0xF3C1, // menu_hl_text
    0x2945, // menu_param
    0x18C3, // freq_text
    0x2945, // funit_text
    0x0000, // freq_hl
    0x4208, // freq_hl_sel
    0x2945, // rds_text
    0x18C3, // scale_text
    0x0000, // scale_pointer
    0x4208, // scale_line
    0x6B4D, // scan_grid
    0x5ACB, // scan_snr
    0x2945, // scan_rssi
  },

  {
    "Night",
    0x0000, // bg
    0xD986, // text
    0xB925, // text_muted
    0xF800, // text_warn
    0xB925, // smeter_icon
    0x8925, // smeter_bar
    0xF800, // smeter_bar_plus
    0x2104, // smeter_bar_empty
    0xF800, // save_icon
    0xB925, // stereo_icon
    0xF800, // rf_icon
    0x8925, // rf_icon_conn
    0xD986, // batt_voltage
    0xD986, // batt_border
    0x8925, // batt_full
    0xF800, // batt_low
    0x0000, // batt_charge
    0xF800, // batt_icon
    0xB925, // band_text
    0xB925, // mode_text
    0xB925, // mode_border
    0x0000, // box_bg
    0xB925, // box_border
    0xB925, // box_text
    0x70C3, // box_off_bg
    0xD986, // box_off_text
    0x0000, // menu_bg
    0xF800, // menu_border
    0xD986, // menu_hdr
    0xF800, // menu_item
    0xD986, // menu_hl_bg
    0x0000, // menu_hl_text
    0xD986, // menu_param
    0xD986, // freq_text
    0xB925, // funit_text
    0xF800, // freq_hl
    0xD986, // freq_hl_sel
    0xB925, // rds_text
    0xD986, // scale_text
    0xF800, // scale_pointer
    0xB925, // scale_line
    0x8925, // scan_grid
    0x8925, // scan_snr
    0xF800, // scan_rssi
  },

  {
    "Phosphor",
    0x0060, // bg
    0x07AD, // text
    0x052D, // text_muted
    0xF800, // text_warn
    0x052D, // smeter_icon
    0x052D, // smeter_bar
    0x07AD, // smeter_bar_plus
    0x00E0, // smeter_bar_empty
    0x2364, // save_icon
    0x052D, // stereo_icon
    0x0309, // rf_icon
    0x052D, // rf_icon_conn
    0x052D, // batt_voltage
    0x07AD, // batt_border
    0x052D, // batt_full
    0x0309, // batt_low
    0x0060, // batt_charge
    0x07AD, // batt_icon
    0x052D, // band_text
    0x052D, // mode_text
    0x052D, // mode_border
    0x0060, // box_bg
    0x052D, // box_border
    0x052D, // box_text
    0x0309, // box_off_bg
    0x07AD, // box_off_text
    0x0060, // menu_bg
    0x2364, // menu_border
    0x07AD, // menu_hdr
    0x052D, // menu_item
    0x0309, // menu_hl_bg
    0x07AD, // menu_hl_text
    0x07AD, // menu_param
    0x07AD, // freq_text
    0x052D, // funit_text
    0x5CF2, // freq_hl
    0x07AD, // freq_hl_sel
    0x052D, // rds_text
    0x07AD, // scale_text
    0x5CF2, // scale_pointer
    0x2364, // scale_line
    0x2364, // scan_grid
    0x2364, // scan_snr
    0x052D, // scan_rssi
  },

  {
    "Space",
    0x0004, // bg
    0x3FE0, // text
    0xD69A, // text_muted
    0xF800, // text_warn
    0xD69A, // smeter_icon
    0x07E0, // smeter_bar
    0xF800, // smeter_bar_plus
    0x3186, // smeter_bar_empty
    0xF800, // save_icon
    0xD69A, // stereo_icon
    0xF800, // rf_icon
    0x07E0, // rf_icon_conn
    0xD69A, // batt_voltage
    0xD69A, // batt_border
    0x07E0, // batt_full
    0xF800, // batt_low
    0x0004, // batt_charge
    0xFFE0, // batt_icon
    0xD69A, // band_text
    0xD69A, // mode_text
    0xD69A, // mode_border
    0x0004, // box_bg
    0xD69A, // box_border
    0xD69A, // box_text
    0xF800, // box_off_bg
    0xBEDF, // box_off_text
    0x0004, // menu_bg
    0xF800, // menu_border
    0x3FE0, // menu_hdr
    0xBEDF, // menu_item
    0x105B, // menu_hl_bg
    0xBEDF, // menu_hl_text
    0xBEDF, // menu_param
    0x3FE0, // freq_text
    0xD69A, // funit_text
    0xF800, // freq_hl
    0xD69A, // freq_hl_sel
    0xD69A, // rds_text
    0x3FE0, // scale_text
    0xF800, // scale_pointer
    0xC638, // scale_line
    0x6B4D, // scan_grid
    0x001F, // scan_snr
    0x07E0, // scan_rssi
  },

  {
    "Magenta",
    0xA12B, // bg
    0xFFFF, // text
    0xFD95, // text_muted
    0xFD00, // text_warn
    0xC638, // smeter_icon
    0xD3F2, // smeter_bar
    0xFD95, // smeter_bar_plus
    0x8829, // smeter_bar_empty
    0x5005, // save_icon
    0xC638, // stereo_icon
    0x7007, // rf_icon
    0xFD95, // rf_icon_conn
    0xC638, // batt_voltage
    0xC638, // batt_border
    0xFD95, // batt_full
    0x7007, // batt_low
    0xA12B, // batt_charge
    0xFFE0, // batt_icon
    0xC638, // band_text
    0xC638, // mode_text
    0xC638, // mode_border
    0xA12B, // box_bg
    0xC638, // box_border
    0xC638, // box_text
    0x7007, // box_off_bg
    0xFD95, // box_off_text
    0xA12B, // menu_bg
    0x5005, // menu_border
    0xFFFF, // menu_hdr
    0xBEDF, // menu_item
    0x5005, // menu_hl_bg
    0xBEDF, // menu_hl_text
    0xBEDF, // menu_param
    0xFFFF, // freq_text
    0xC638, // funit_text
    0x5005, // freq_hl
    0xFFFF, // freq_hl_sel
    0xFD95, // rds_text
    0xFFFF, // scale_text
    0x5005, // scale_pointer
    0xC638, // scale_line
    0xD3F2, // scan_grid
    0xD3F2, // scan_snr
    0xFD95, // scan_rssi
  },

  {
    // NOTE: This entry is dynamically overwritten by applyCustomTheme() at startup.
    // It MUST remain the last entry so that getTotalThemes()-1 correctly identifies it.
    "Custom",
    0x0000, // bg           (placeholder)
    0xFFFF, // text
    0xD69A, // text_muted
    0xF800, // text_warn
    0xD69A, // smeter_icon
    0x07E0, // smeter_bar
    0xF800, // smeter_bar_plus
    0x3186, // smeter_bar_empty
    0xF800, // save_icon
    0xD69A, // stereo_icon
    0xF800, // rf_icon
    0x07E0, // rf_icon_conn
    0xFFFF, // batt_voltage
    0xFFFF, // batt_border
    0x07E0, // batt_full
    0xF800, // batt_low
    0x0000, // batt_charge
    0xFFE0, // batt_icon
    0xD69A, // band_text
    0xD69A, // mode_text
    0xD69A, // mode_border
    0x0000, // box_bg
    0xD69A, // box_border
    0xD69A, // box_text
    0xF800, // box_off_bg
    0xBEDF, // box_off_text
    0x0000, // menu_bg
    0xF800, // menu_border
    0xFFFF, // menu_hdr
    0xBEDF, // menu_item
    0x105B, // menu_hl_bg
    0xBEDF, // menu_hl_text
    0xBEDF, // menu_param
    0xFFFF, // freq_text
    0xD69A, // funit_text
    0xF800, // freq_hl
    0xFFE0, // freq_hl_sel
    0xD69A, // rds_text
    0xFFFF, // scale_text
    0xF800, // scale_pointer
    0xC638, // scale_line
    0x94B2, // scan_grid
    0x0659, // scan_snr
    0x07E0, // scan_rssi
  },
};

uint8_t themeIdx = 0;
int getTotalThemes() { return(ITEM_COUNT(theme)); }

//
// Turn theme editor on (1) or off (0), or get current status (2)
//
bool switchThemeEditor(int8_t state)
{
  static bool themeEditor = false;
  themeEditor = state == 0 ? false : (state == 1 ? true : themeEditor);
  return themeEditor;
}

// Current palette index for the Custom theme slot
uint8_t customPaletteIdx = 0;

// ---------------------------------------------------------------------------
// Curated palette definitions: each entry supplies three seed colors in HSV.
//   bg  — dark background
//   txt — primary text / frequency display
//   acc — accent / highlight
// Muted text and the highlight-bg are derived automatically in applyCustomTheme.
// ---------------------------------------------------------------------------
typedef struct {
  const char *name;
  float bgH,  bgS,  bgV;   // background
  float txtH, txtS, txtV;  // primary text / freq
  float accH, accS, accV;  // accent
} PaletteDef;

static const PaletteDef kPalettes[] = {
  // name          bgH   bgS   bgV    txtH  txtS  txtV    accH  accS  accV
  { "Aurora",      265, 0.85, 0.12,   175, 0.90, 0.90,   120, 0.95, 0.85 },
  { "Sunset",      345, 0.90, 0.12,    32, 1.00, 0.95,    10, 1.00, 1.00 },
  { "Phosphor",    190, 0.90, 0.10,   120, 0.85, 0.90,    85, 1.00, 0.95 },
  { "Deep Sea",    222, 0.90, 0.12,   200, 0.80, 1.00,    45, 1.00, 1.00 },
  { "Forest",      130, 0.90, 0.10,    80, 0.85, 0.90,    55, 1.00, 1.00 },
  { "Midnight",    240, 0.75, 0.13,   260, 0.30, 1.00,   290, 0.95, 1.00 },
  { "Volcano",      20, 0.30, 0.08,    22, 1.00, 1.00,     8, 1.00, 0.95 },
  { "Arctic",      205, 0.50, 0.15,   200, 0.15, 1.00,   195, 0.90, 1.00 },
  { "Sakura",      300, 0.65, 0.13,   340, 0.40, 1.00,   350, 0.90, 1.00 },
  { "Gold Rush",    30, 0.80, 0.10,    48, 0.95, 1.00,    28, 1.00, 0.95 },
  { "Volt",          0, 0.00, 0.05,    68, 1.00, 1.00,   120, 1.00, 1.00 },
  { "Twilight",    250, 0.70, 0.13,    45, 0.80, 0.95,   270, 0.90, 1.00 },
  { "Copper",       20, 0.20, 0.09,    20, 0.90, 0.95,    38, 1.00, 1.00 },
  { "Jade",        160, 0.60, 0.10,   155, 0.70, 0.90,    90, 1.00, 0.90 },
  { "Crimson",     208, 0.65, 0.13,     5, 0.90, 0.95,    15, 0.80, 1.00 },
  { "Glacier",     210, 0.25, 0.10,   200, 0.65, 1.00,     0, 0.00, 1.00 },
};

int getCustomPaletteCount()
{
  return (int)ITEM_COUNT(kPalettes);
}

const char *getCustomPaletteName(uint8_t idx)
{
  if(idx >= (uint8_t)ITEM_COUNT(kPalettes)) idx = 0;
  return kPalettes[idx].name;
}

// Convert HSV (h=0..360, s=0..1, v=0..1) to RGB565
uint16_t hsvToRgb565(float h, float s, float v)
{
  float r, g, b;
  if(s <= 0.0f) { r = g = b = v; }
  else
  {
    float hh = h / 60.0f;
    int   i  = (int)hh;
    float ff = hh - (float)i;
    float p  = v * (1.0f - s);
    float q  = v * (1.0f - s * ff);
    float t  = v * (1.0f - s * (1.0f - ff));
    switch(i % 6)
    {
      case 0:  r = v; g = t; b = p; break;
      case 1:  r = q; g = v; b = p; break;
      case 2:  r = p; g = v; b = t; break;
      case 3:  r = p; g = q; b = v; break;
      case 4:  r = t; g = p; b = v; break;
      default: r = v; g = p; b = q; break;
    }
  }
  uint8_t r8 = (uint8_t)(r * 255.0f);
  uint8_t g8 = (uint8_t)(g * 255.0f);
  uint8_t b8 = (uint8_t)(b * 255.0f);
  return (uint16_t)(((r8 >> 3) << 11) | ((g8 >> 2) << 5) | (b8 >> 3));
}

// Return white (0xFFFF) or black (0x0000) depending on bg luminance
uint16_t contrastingTextColor(uint16_t bg565)
{
  uint8_t r = (bg565 >> 11) << 3;
  uint8_t g = ((bg565 >> 5) & 0x3F) << 2;
  uint8_t b = (bg565 & 0x1F) << 3;
  // Weighted luminance (sRGB approximation, scaled to 0-65535)
  uint32_t lum = (uint32_t)r * 77u + (uint32_t)g * 150u + (uint32_t)b * 29u;
  return (lum > 128u * 256u) ? 0x0000u : 0xFFFFu;
}

// Rebuild the Custom theme slot from a curated palette index
void applyCustomTheme(uint8_t idx)
{
  if(idx >= (uint8_t)ITEM_COUNT(kPalettes)) idx = 0;
  customPaletteIdx = idx;

  const PaletteDef &p = kPalettes[idx];

  // Derive the five colour roles from the three palette seeds
  uint16_t bg    = hsvToRgb565(p.bgH, p.bgS, p.bgV);
  uint16_t txt   = hsvToRgb565(p.txtH, p.txtS, p.txtV);
  uint16_t muted = hsvToRgb565(p.txtH, p.txtS * 0.50f, p.txtV * 0.58f);
  uint16_t acc   = hsvToRgb565(p.accH, p.accS, p.accV);
  // Highlight bg: same hue as bg but lifted to a mid-tone
  float hlV      = fminf(p.bgV * 4.0f, 0.42f);
  uint16_t hlbg  = hsvToRgb565(p.bgH, p.bgS * 0.75f, hlV);
  uint16_t hltxt = contrastingTextColor(hlbg);
  uint16_t brt   = acc;  // alias — used for mid-bright menu items

  // Custom is always the last entry in theme[]
  ColorTheme *t = &theme[getTotalThemes() - 1];

  t->bg               = bg;
  t->text             = txt;
  t->text_muted       = muted;
  t->text_warn        = 0xF800;
  t->smeter_icon      = muted;
  t->smeter_bar       = acc;
  t->smeter_bar_plus  = 0xF800;
  t->smeter_bar_empty = hlbg;
  t->save_icon        = acc;
  t->stereo_icon      = muted;
  t->rf_icon          = 0xF800;
  t->rf_icon_conn     = acc;
  t->batt_voltage     = txt;
  t->batt_border      = txt;
  t->batt_full        = acc;
  t->batt_low         = 0xF800;
  t->batt_charge      = bg;
  t->batt_icon        = brt;
  t->band_text        = muted;
  t->mode_text        = muted;
  t->mode_border      = muted;
  t->box_bg           = bg;
  t->box_border       = muted;
  t->box_text         = muted;
  t->box_off_bg       = 0xF800;
  t->box_off_text     = brt;
  t->menu_bg          = bg;
  t->menu_border      = acc;
  t->menu_hdr         = txt;
  t->menu_item        = brt;
  t->menu_hl_bg       = hlbg;
  t->menu_hl_text     = hltxt;
  t->menu_param       = brt;
  t->freq_text        = txt;
  t->funit_text       = muted;
  t->freq_hl          = acc;
  t->freq_hl_sel      = brt;
  t->rds_text         = muted;
  t->scale_text       = txt;
  t->scale_pointer    = acc;
  t->scale_line       = muted;
  t->scan_grid        = hlbg;
  t->scan_snr         = brt;
  t->scan_rssi        = acc;
}
