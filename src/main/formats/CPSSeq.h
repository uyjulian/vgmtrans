#pragma once
#include "VGMSeq.h"
#include "SeqTrack.h"
#include "CPSFormat.h"

enum CPSFormatVer: uint8_t;


static const uint8_t delta_table[3][7] = {2, 4, 8, 0x10, 0x20, 0x40, 0x80,
                                          3, 6, 0xC, 0x18, 0x30, 0x60, 0xC0,
                                          0, 9, 0x12, 0x24, 0x48, 0x90, 0};
//octave_table provides the note value for the start of each octave.
//wholly unnecessary for me include it, but i'm following the original driver code verbatim for now
static const uint8_t octave_table[] = {0x00, 0x0C, 0x18, 0x24, 0x30, 0x3C, 0x48, 0x54,
                                       0x18, 0x24, 0x30, 0x3C, 0x48, 0x54, 0x60, 0x6C};

static const uint16_t vol_table[128] = {
    0, 0xA, 0x18, 0x26, 0x34, 0x42, 0x51, 0x5F, 0x6E, 0x7D, 0x8C, 0x9B, 0xAA,
    0xBA, 0xC9, 0xD9, 0xE9, 0xF8, 0x109, 0x119, 0x129, 0x13A, 0x14A, 0x15B,
    0x16C, 0x17D, 0x18E, 0x1A0, 0x1B2, 0x1C3, 0x1D5, 0x1E8, 0x1FC, 0x20D, 0x21F,
    0x232, 0x245, 0x259, 0x26C, 0x280, 0x294, 0x2A8, 0x2BD, 0x2D2, 0x2E7, 0x2FC,
    0x311, 0x327, 0x33D, 0x353, 0x36A, 0x381, 0x398, 0x3B0, 0x3C7, 0x3DF, 0x3F8,
    0x411, 0x42A, 0x443, 0x45D, 0x477, 0x492, 0x4AD, 0x4C8, 0x4E4, 0x501, 0x51D,
    0x53B, 0x558, 0x577, 0x596, 0x5B5, 0x5D5, 0x5F5, 0x616, 0x638, 0x65A, 0x67D,
    0x6A1, 0x6C5, 0x6EB, 0x711, 0x738, 0x75F, 0x788, 0x7B2, 0x7DC, 0x808, 0x834,
    0x862, 0x891, 0x8C2, 0x8F3, 0x927, 0x95B, 0x991, 0x9C9, 0xA03, 0xA3F,
    0xA7D, 0xABD, 0xAFF, 0xB44, 0xB8C, 0xBD7, 0xC25, 0xC76, 0xCCC,
    0xD26, 0xD85, 0xDE9, 0xE53, 0xEC4, 0xF3C, 0xFBD, 0x1048, 0x10DF,
    0x1184, 0x123A, 0x1305, 0x13EA, 0x14F1, 0x1625, 0x179B, 0x1974, 0x1BFB, 0x1FFD
};

// Note that 0x100 = 1 semitone.  so the lower byte represents x / 256 cents
static const uint16_t vibrato_depth_table[128] = {
    0x0, 0xC, 0xD, 0xE, 0xE, 0xF, 0x10, 0x11, 0x12, 0x12, 0x13, 0x14, 0x15,
    0x16, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1C, 0x1D, 0x1F, 0x20, 0x22, 0x24, 0x25,
    0x27, 0x29, 0x2A, 0x2C, 0x2D, 0x2F, 0x31, 0x32, 0x34, 0x38, 0x3B, 0x3E, 0x41,
    0x45, 0x48, 0x4B, 0x4E, 0x52, 0x55, 0x58, 0x5B, 0x5F, 0x62, 0x65, 0x6A, 0x72,
    0x7A, 0x82, 0x8A, 0x92, 0x9A, 0xA2, 0xAA, 0xB2, 0xBA, 0xC2, 0xCA, 0xD2,
    0xDA, 0xE2, 0xEA, 0xF2, 0xFA, 0x104, 0x114, 0x124, 0x134, 0x144, 0x154,
    0x164, 0x174, 0x184, 0x194, 0x1A4, 0x1B4, 0x1C4, 0x1D4, 0x1E4, 0x1F4, 0x208,
    0x228, 0x248, 0x268, 0x288, 0x2A8, 0x2C8, 0x2E8, 0x308, 0x328, 0x348, 0x368,
    0x388, 0x3A8, 0x3C8, 0x3E8, 0x410, 0x450, 0x490, 0x4D0, 0x510, 0x550, 0x590,
    0x5D0, 0x610, 0x650, 0x690, 0x6D0, 0x710, 0x750, 0x790, 0x7D0, 0x828, 0x888,
    0x8E8, 0x948, 0x9A8, 0xA08, 0xA68, 0xAC8, 0xB28, 0xB88, 0xBE8
};

static const uint16_t tremelo_depth_table[128] = {
    0, 0x1B9, 0x2B5, 0x3B1, 0x4AD, 0x5A9, 0x6A5, 0x7A1, 0x89D, 0x999, 0xA95,
    0xB91, 0xC8D, 0xD89, 0xE85, 0xF81, 0x107D, 0x1179, 0x1275, 0x1371,
    0x146D, 0x1569, 0x1665, 0x1761, 0x185D, 0x1959, 0x1A55, 0x1B51, 0x1C4D,
    0x1D49, 0x1E45, 0x1F41, 0x203D, 0x2139, 0x2235, 0x2331, 0x242D, 0x2529,
    0x2625, 0x2721, 0x281D, 0x2919, 0x2A15, 0x2B11, 0x2C0D, 0x2D09, 0x2E05,
    0x2F01, 0x2FFD, 0x30F9, 0x31F5, 0x32F1, 0x33ED, 0x34E9, 0x35E5, 0x36E1,
    0x37DD, 0x38D9, 0x39D5, 0x3AD1, 0x3BCD, 0x3CC9, 0x3DC5, 0x3EC1, 0x40FD,
    0x42F9, 0x44F5, 0x46F1, 0x48ED, 0x4AE9, 0x4CE5, 0x4EE1, 0x50DD, 0x52D9,
    0x54D5, 0x56D1, 0x58CD, 0x5AC9, 0x5CC5, 0x5EC1, 0x60BD, 0x62B9, 0x64B5,
    0x66B1, 0x68AD, 0x6AA9, 0x6CA5, 0x6EA1, 0x709D, 0x7299, 0x7495, 0x7691,
    0x788D, 0x7A89, 0x7C85, 0x7E81, 0x827D, 0x8679, 0x8A75, 0x8E71, 0x926D,
    0x9669, 0x9A65, 0x9E61, 0xA25D, 0xA659, 0xAA55, 0xAE51, 0xB24D,
    0xB649, 0xBA45, 0xBE41, 0xC23D, 0xC639, 0xCA35, 0xCE31, 0xD22D,
    0xD629, 0xDA25, 0xDE21, 0xE21D, 0xE619, 0xEA15, 0xEE11, 0xF20D,
    0xF609, 0xFA05, 0xFE01
};

static const uint16_t lfo_rate_table[128] = {
    0, 0x118, 0x126, 0x133, 0x142, 0x150, 0x15D, 0x16C, 0x179, 0x187, 0x196,
    0x1A3, 0x1B1, 0x1C0, 0x1DB, 0x1F7, 0x213, 0x22F, 0x24B, 0x267, 0x283, 0x29F,
    0x2BB, 0x2D7, 0x2F3, 0x30F, 0x32B, 0x347, 0x363, 0x37F, 0x3B7, 0x3EF, 0x427,
    0x45F, 0x496, 0x4CF, 0x506, 0x53E, 0x576, 0x5AE, 0x5E6, 0x61E, 0x656, 0x68E,
    0x6C6, 0x6FE, 0x76E, 0x7DE, 0x84D, 0x8BD, 0x92D, 0x99D, 0xA0D, 0xA7D,
    0xAEC, 0xB5C, 0xBCC, 0xC3C, 0xCAC, 0xD1C, 0xD8C, 0xDFB, 0xEDB,
    0xFBB, 0x109B, 0x117A, 0x125A, 0x133A, 0x141A, 0x14F9, 0x15D9, 0x16B9,
    0x1799, 0x1878, 0x1958, 0x1A38, 0x1B17, 0x1BF7, 0x1DB6, 0x1F76, 0x2135,
    0x22F5, 0x24B4, 0x2674, 0x2833, 0x29F3, 0x2BB2, 0x2D71, 0x2F31, 0x30F0,
    0x32B0, 0x346F, 0x362F, 0x37EE, 0x3B6D, 0x3EEC, 0x426B, 0x45EA, 0x4969,
    0x4CE8, 0x5066, 0x53E5, 0x5764, 0x5AE3, 0x5E62, 0x61E1, 0x6550, 0x68DE,
    0x6C5E, 0x6FDC, 0x76DA, 0x7DD8, 0x84D6, 0x8BD3, 0x92D1, 0x99CF, 0xA0CD,
    0xA7CB, 0xAEC8, 0xB5C6, 0xBCC4, 0xC3C2, 0xCABF, 0xD1BD, 0xD8BB,
    0xDFB9, 0xEDB4, 0xFBB0
};

class CPSSeq:
    public VGMSeq {
public:
  CPSSeq(RawFile *file, uint32_t offset, CPSFormatVer fmt_version, std::wstring &name);
  virtual ~CPSSeq(void);

  virtual bool GetHeaderInfo(void);
  virtual bool GetTrackPointers(void);
  virtual bool PostLoad(void);

public:
  CPSFormatVer fmt_version;
};
