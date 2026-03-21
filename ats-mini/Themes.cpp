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
    // Dark flat — deep navy + ice-blue text + electric-blue accent
    "Cobalt",
    0x0843, // bg
    0xDF5F, // text
    0x6394, // text_muted
    0xF800, // text_warn
    0x6394, // smeter_icon
    0x349F, // smeter_bar
    0xF800, // smeter_bar_plus
    0x1109, // smeter_bar_empty
    0x349F, // save_icon
    0x6394, // stereo_icon
    0xF800, // rf_icon
    0x349F, // rf_icon_conn
    0xDF5F, // batt_voltage
    0xDF5F, // batt_border
    0x349F, // batt_full
    0xF800, // batt_low
    0x0843, // batt_charge
    0x6394, // batt_icon
    0x6394, // band_text
    0x6394, // mode_text
    0x6394, // mode_border
    0x0843, // box_bg
    0x6394, // box_border
    0x6394, // box_text
    0xF800, // box_off_bg
    0x6394, // box_off_text
    0x0843, // menu_bg
    0x349F, // menu_border
    0xDF5F, // menu_hdr
    0x6394, // menu_item
    0x1109, // menu_hl_bg
    0xFFFF, // menu_hl_text
    0x6394, // menu_param
    0xDF5F, // freq_text
    0x6394, // funit_text
    0x349F, // freq_hl
    0x6394, // freq_hl_sel
    0x6394, // rds_text
    0xDF5F, // scale_text
    0x349F, // scale_pointer
    0x6394, // scale_line
    0x1109, // scan_grid
    0x6394, // scan_snr
    0x349F, // scan_rssi
  },

  {
    // Dark flat — near-black + electric cyan + hot pink accent
    "Neon",
    0x0041, // bg
    0x07DB, // text
    0x040C, // text_muted
    0xF800, // text_warn
    0x040C, // smeter_icon
    0xF810, // smeter_bar
    0xF800, // smeter_bar_plus
    0x10A3, // smeter_bar_empty
    0xF810, // save_icon
    0x040C, // stereo_icon
    0xF800, // rf_icon
    0xF810, // rf_icon_conn
    0x07DB, // batt_voltage
    0x07DB, // batt_border
    0xF810, // batt_full
    0xF800, // batt_low
    0x0041, // batt_charge
    0x040C, // batt_icon
    0x040C, // band_text
    0x040C, // mode_text
    0x040C, // mode_border
    0x0041, // box_bg
    0x040C, // box_border
    0x040C, // box_text
    0xF800, // box_off_bg
    0x040C, // box_off_text
    0x0041, // menu_bg
    0xF810, // menu_border
    0x07DB, // menu_hdr
    0x040C, // menu_item
    0x10A3, // menu_hl_bg
    0xFFFF, // menu_hl_text
    0x040C, // menu_param
    0x07DB, // freq_text
    0x040C, // funit_text
    0xF810, // freq_hl
    0x040C, // freq_hl_sel
    0x040C, // rds_text
    0x07DB, // scale_text
    0xF810, // scale_pointer
    0x040C, // scale_line
    0x10A3, // scan_grid
    0x040C, // scan_snr
    0xF810, // scan_rssi
  },

  {
    // Dark flat — dark olive-black + warm amber + burnt orange accent
    "Autumn",
    0x1081, // bg
    0xFE4A, // text
    0xA384, // text_muted
    0xF800, // text_warn
    0xA384, // smeter_icon
    0xF300, // smeter_bar
    0xF800, // smeter_bar_plus
    0x3121, // smeter_bar_empty
    0xF300, // save_icon
    0xA384, // stereo_icon
    0xF800, // rf_icon
    0xF300, // rf_icon_conn
    0xFE4A, // batt_voltage
    0xFE4A, // batt_border
    0xF300, // batt_full
    0xF800, // batt_low
    0x1081, // batt_charge
    0xA384, // batt_icon
    0xA384, // band_text
    0xA384, // mode_text
    0xA384, // mode_border
    0x1081, // box_bg
    0xA384, // box_border
    0xA384, // box_text
    0xF800, // box_off_bg
    0xA384, // box_off_text
    0x1081, // menu_bg
    0xF300, // menu_border
    0xFE4A, // menu_hdr
    0xA384, // menu_item
    0x3121, // menu_hl_bg
    0xFFFF, // menu_hl_text
    0xA384, // menu_param
    0xFE4A, // freq_text
    0xA384, // funit_text
    0xF300, // freq_hl
    0xA384, // freq_hl_sel
    0xA384, // rds_text
    0xFE4A, // scale_text
    0xF300, // scale_pointer
    0xA384, // scale_line
    0x3121, // scan_grid
    0xA384, // scan_snr
    0xF300, // scan_rssi
  },

  {
    // Dark gradient — deep purple-black + warm gold + violet accent
    "Grape",
    0x0822, // bg
    0xFE91, // text
    0x9336, // text_muted
    0xF800, // text_warn
    0x9336, // smeter_icon
    0xB29F, // smeter_bar
    0xF800, // smeter_bar_plus
    0x1886, // smeter_bar_empty
    0xB29F, // save_icon
    0x9336, // stereo_icon
    0xF800, // rf_icon
    0xB29F, // rf_icon_conn
    0xFE91, // batt_voltage
    0xFE91, // batt_border
    0xB29F, // batt_full
    0xF800, // batt_low
    0x0822, // batt_charge
    0x9336, // batt_icon
    0x9336, // band_text
    0x9336, // mode_text
    0x9336, // mode_border
    0x0822, // box_bg
    0x9336, // box_border
    0x9336, // box_text
    0xF800, // box_off_bg
    0x9336, // box_off_text
    0x0822, // menu_bg
    0xB29F, // menu_border
    0xFE91, // menu_hdr
    0x9336, // menu_item
    0x1886, // menu_hl_bg
    0xFFFF, // menu_hl_text
    0x9336, // menu_param
    0xFE91, // freq_text
    0x9336, // funit_text
    0xB29F, // freq_hl
    0x9336, // freq_hl_sel
    0x9336, // rds_text
    0xFE91, // scale_text
    0xB29F, // scale_pointer
    0x9336, // scale_line
    0x1886, // scan_grid
    0x9336, // scan_snr
    0xB29F, // scan_rssi
  },

  {
    // Dark flat — very dark espresso brown + warm cream + caramel accent
    "Espresso",
    0x1061, // bg
    0xF718, // text
    0xA3CA, // text_muted
    0xF800, // text_warn
    0xA3CA, // smeter_icon
    0xCBC5, // smeter_bar
    0xF800, // smeter_bar_plus
    0x28E2, // smeter_bar_empty
    0xCBC5, // save_icon
    0xA3CA, // stereo_icon
    0xF800, // rf_icon
    0xCBC5, // rf_icon_conn
    0xF718, // batt_voltage
    0xF718, // batt_border
    0xCBC5, // batt_full
    0xF800, // batt_low
    0x1061, // batt_charge
    0xA3CA, // batt_icon
    0xA3CA, // band_text
    0xA3CA, // mode_text
    0xA3CA, // mode_border
    0x1061, // box_bg
    0xA3CA, // box_border
    0xA3CA, // box_text
    0xF800, // box_off_bg
    0xA3CA, // box_off_text
    0x1061, // menu_bg
    0xCBC5, // menu_border
    0xF718, // menu_hdr
    0xA3CA, // menu_item
    0x28E2, // menu_hl_bg
    0xFFFF, // menu_hl_text
    0xA3CA, // menu_param
    0xF718, // freq_text
    0xA3CA, // funit_text
    0xCBC5, // freq_hl
    0xA3CA, // freq_hl_sel
    0xA3CA, // rds_text
    0xF718, // scale_text
    0xCBC5, // scale_pointer
    0xA3CA, // scale_line
    0x28E2, // scan_grid
    0xA3CA, // scan_snr
    0xCBC5, // scan_rssi
  },

  {
    // Light flat — warm parchment + dark brown text + terracotta accent
    "Paper",
    0xFF9B, // bg
    0x28C1, // text
    0x82C8, // text_muted
    0xF800, // text_warn
    0x82C8, // smeter_icon
    0xC203, // smeter_bar
    0xF800, // smeter_bar_plus
    0xDE55, // smeter_bar_empty
    0xC203, // save_icon
    0x82C8, // stereo_icon
    0xF800, // rf_icon
    0xC203, // rf_icon_conn
    0x28C1, // batt_voltage
    0x28C1, // batt_border
    0xC203, // batt_full
    0xF800, // batt_low
    0xFF9B, // batt_charge
    0x82C8, // batt_icon
    0x82C8, // band_text
    0x82C8, // mode_text
    0x82C8, // mode_border
    0xFF9B, // box_bg
    0x82C8, // box_border
    0x82C8, // box_text
    0xF800, // box_off_bg
    0x82C8, // box_off_text
    0xFF9B, // menu_bg
    0xC203, // menu_border
    0x28C1, // menu_hdr
    0x82C8, // menu_item
    0xDE55, // menu_hl_bg
    0x0000, // menu_hl_text
    0x82C8, // menu_param
    0x28C1, // freq_text
    0x82C8, // funit_text
    0xC203, // freq_hl
    0x82C8, // freq_hl_sel
    0x82C8, // rds_text
    0x28C1, // scale_text
    0xC203, // scale_pointer
    0x82C8, // scale_line
    0xDE55, // scan_grid
    0x82C8, // scan_snr
    0xC203, // scan_rssi
  },

  {
    // Light flat — pale seafoam + dark teal text + forest green accent
    "Mint",
    0xE7BD, // bg
    0x0944, // text
    0x438C, // text_muted
    0xF800, // text_warn
    0x438C, // smeter_icon
    0x0408, // smeter_bar
    0xF800, // smeter_bar_plus
    0xB6F9, // smeter_bar_empty
    0x0408, // save_icon
    0x438C, // stereo_icon
    0xF800, // rf_icon
    0x0408, // rf_icon_conn
    0x0944, // batt_voltage
    0x0944, // batt_border
    0x0408, // batt_full
    0xF800, // batt_low
    0xE7BD, // batt_charge
    0x438C, // batt_icon
    0x438C, // band_text
    0x438C, // mode_text
    0x438C, // mode_border
    0xE7BD, // box_bg
    0x438C, // box_border
    0x438C, // box_text
    0xF800, // box_off_bg
    0x438C, // box_off_text
    0xE7BD, // menu_bg
    0x0408, // menu_border
    0x0944, // menu_hdr
    0x438C, // menu_item
    0xB6F9, // menu_hl_bg
    0x0000, // menu_hl_text
    0x438C, // menu_param
    0x0944, // freq_text
    0x438C, // funit_text
    0x0408, // freq_hl
    0x438C, // freq_hl_sel
    0x438C, // rds_text
    0x0944, // scale_text
    0x0408, // scale_pointer
    0x438C, // scale_line
    0xB6F9, // scan_grid
    0x438C, // scan_snr
    0x0408, // scan_rssi
  },

  {
    // Light gradient — pale lavender + deep indigo text + violet accent
    "Lavender",
    0xEF3F, // bg
    0x1888, // text
    0x62D2, // text_muted
    0xF800, // text_warn
    0x62D2, // smeter_icon
    0x6018, // smeter_bar
    0xF800, // smeter_bar_plus
    0xCE1D, // smeter_bar_empty
    0x6018, // save_icon
    0x62D2, // stereo_icon
    0xF800, // rf_icon
    0x6018, // rf_icon_conn
    0x1888, // batt_voltage
    0x1888, // batt_border
    0x6018, // batt_full
    0xF800, // batt_low
    0xEF3F, // batt_charge
    0x62D2, // batt_icon
    0x62D2, // band_text
    0x62D2, // mode_text
    0x62D2, // mode_border
    0xEF3F, // box_bg
    0x62D2, // box_border
    0x62D2, // box_text
    0xF800, // box_off_bg
    0x62D2, // box_off_text
    0xEF3F, // menu_bg
    0x6018, // menu_border
    0x1888, // menu_hdr
    0x62D2, // menu_item
    0xCE1D, // menu_hl_bg
    0x0000, // menu_hl_text
    0x62D2, // menu_param
    0x1888, // freq_text
    0x62D2, // funit_text
    0x6018, // freq_hl
    0x62D2, // freq_hl_sel
    0x62D2, // rds_text
    0x1888, // scale_text
    0x6018, // scale_pointer
    0x62D2, // scale_line
    0xCE1D, // scan_grid
    0x62D2, // scan_snr
    0x6018, // scan_rssi
  },

  {
    // Light gradient — warm blush + dark crimson text + vivid red-orange accent
    "Coral",
    0xFF5C, // bg
    0x4003, // text
    0xB30C, // text_muted
    0xF800, // text_warn
    0xB30C, // smeter_icon
    0xD984, // smeter_bar
    0xF800, // smeter_bar_plus
    0xF699, // smeter_bar_empty
    0xD984, // save_icon
    0xB30C, // stereo_icon
    0xF800, // rf_icon
    0xD984, // rf_icon_conn
    0x4003, // batt_voltage
    0x4003, // batt_border
    0xD984, // batt_full
    0xF800, // batt_low
    0xFF5C, // batt_charge
    0xB30C, // batt_icon
    0xB30C, // band_text
    0xB30C, // mode_text
    0xB30C, // mode_border
    0xFF5C, // box_bg
    0xB30C, // box_border
    0xB30C, // box_text
    0xF800, // box_off_bg
    0xB30C, // box_off_text
    0xFF5C, // menu_bg
    0xD984, // menu_border
    0x4003, // menu_hdr
    0xB30C, // menu_item
    0xF699, // menu_hl_bg
    0x0000, // menu_hl_text
    0xB30C, // menu_param
    0x4003, // freq_text
    0xB30C, // funit_text
    0xD984, // freq_hl
    0xB30C, // freq_hl_sel
    0xB30C, // rds_text
    0x4003, // scale_text
    0xD984, // scale_pointer
    0xB30C, // scale_line
    0xF699, // scan_grid
    0xB30C, // scan_snr
    0xD984, // scan_rssi
  },

  {
    // Light flat — cool grey + dark navy text + cobalt blue accent
    "Slate",
    0xF7BF, // bg
    0x10C6, // text
    0x5B51, // text_muted
    0xF800, // text_warn
    0x5B51, // smeter_icon
    0x0298, // smeter_bar
    0xF800, // smeter_bar_plus
    0xCE9B, // smeter_bar_empty
    0x0298, // save_icon
    0x5B51, // stereo_icon
    0xF800, // rf_icon
    0x0298, // rf_icon_conn
    0x10C6, // batt_voltage
    0x10C6, // batt_border
    0x0298, // batt_full
    0xF800, // batt_low
    0xF7BF, // batt_charge
    0x5B51, // batt_icon
    0x5B51, // band_text
    0x5B51, // mode_text
    0x5B51, // mode_border
    0xF7BF, // box_bg
    0x5B51, // box_border
    0x5B51, // box_text
    0xF800, // box_off_bg
    0x5B51, // box_off_text
    0xF7BF, // menu_bg
    0x0298, // menu_border
    0x10C6, // menu_hdr
    0x5B51, // menu_item
    0xCE9B, // menu_hl_bg
    0x0000, // menu_hl_text
    0x5B51, // menu_param
    0x10C6, // freq_text
    0x5B51, // funit_text
    0x0298, // freq_hl
    0x5B51, // freq_hl_sel
    0x5B51, // rds_text
    0x10C6, // scale_text
    0x0298, // scale_pointer
    0x5B51, // scale_line
    0xCE9B, // scan_grid
    0x5B51, // scan_snr
    0x0298, // scan_rssi
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
