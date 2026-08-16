// PaperCam — Seeed GFX panel configuration
//
// Seeed GFX looks for a sketch-local "driver.h" and pulls it in ahead of its
// own defaults, which is why this lives next to the .ino rather than inside
// the library folder. Editing the library's User_Setup.h would work too, but
// it would not survive a library update and would leak into other sketches.
//
// Values below are from Seeed's Arduino cookbook for the XIAO 7.5" ePaper
// Panel, which pairs this exact panel with this exact carrier board.
//
//   https://wiki.seeedstudio.com/xiao_075inch_epaper_panel_arduino/
//
// 502 selects the 7.5" 800x480 monochrome panel (UC8179 controller).
#define BOARD_SCREEN_COMBO 502

// The carrier board. Note: the Driver Board's own wiki page shows
// USE_XIAO_EPAPER_BREAKOUT_BOARD in its example — that is a copy-paste error
// from the Breakout Board page. The Breakout Board puts BUSY on D5; ours puts
// it on D2. Using the wrong macro here means BUSY is read from an unconnected
// pin and the panel appears to hang forever.
#define USE_XIAO_EPAPER_DRIVER_BOARD
