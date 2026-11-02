// Maps 9300: the 3D renderer (see r3d.h). Portable C++, integers only.

#include "r3d.h"
#include "fastmem.h"
#include "r3dspan.h"
#include "r3dgeo.h"

#define R3_NEAR 4               // the near plane: 0.25 m
#define R3_MAXPOLY 5000            // 0.44: 4000 (Mapy.com's 3D: ~2000 triangles a tile; 200 B each); 0.97: 5000 (1 MB); 1.10: the first size (SetMax resizes)
#define R3_BUCKETS 1024
#define R3_MAXH 400
#define R3_RECIP 8192          // 0.59: 2^30 / n for n < this (the ARM9 has no divide: ~40-100 instructions each)

static const short KSin[1025] = {
    0, 25, 50, 75, 101, 126, 151, 176, 201, 226, 251, 276, 302, 327, 352, 377,
    402, 427, 452, 477, 503, 528, 553, 578, 603, 628, 653, 678, 704, 729, 754, 779,
    804, 829, 854, 879, 904, 929, 955, 980, 1005, 1030, 1055, 1080, 1105, 1130, 1155, 1180,
    1205, 1230, 1255, 1280, 1306, 1331, 1356, 1381, 1406, 1431, 1456, 1481, 1506, 1531, 1556, 1581,
    1606, 1631, 1656, 1681, 1706, 1731, 1756, 1781, 1806, 1831, 1856, 1881, 1906, 1931, 1956, 1981,
    2006, 2031, 2055, 2080, 2105, 2130, 2155, 2180, 2205, 2230, 2255, 2280, 2305, 2329, 2354, 2379,
    2404, 2429, 2454, 2479, 2503, 2528, 2553, 2578, 2603, 2628, 2652, 2677, 2702, 2727, 2752, 2776,
    2801, 2826, 2851, 2875, 2900, 2925, 2949, 2974, 2999, 3024, 3048, 3073, 3098, 3122, 3147, 3172,
    3196, 3221, 3246, 3270, 3295, 3320, 3344, 3369, 3393, 3418, 3442, 3467, 3492, 3516, 3541, 3565,
    3590, 3614, 3639, 3663, 3688, 3712, 3737, 3761, 3786, 3810, 3835, 3859, 3883, 3908, 3932, 3957,
    3981, 4005, 4030, 4054, 4078, 4103, 4127, 4151, 4176, 4200, 4224, 4249, 4273, 4297, 4321, 4346,
    4370, 4394, 4418, 4442, 4467, 4491, 4515, 4539, 4563, 4587, 4612, 4636, 4660, 4684, 4708, 4732,
    4756, 4780, 4804, 4828, 4852, 4876, 4900, 4924, 4948, 4972, 4996, 5020, 5044, 5068, 5092, 5115,
    5139, 5163, 5187, 5211, 5235, 5259, 5282, 5306, 5330, 5354, 5377, 5401, 5425, 5449, 5472, 5496,
    5520, 5543, 5567, 5591, 5614, 5638, 5661, 5685, 5708, 5732, 5756, 5779, 5803, 5826, 5850, 5873,
    5897, 5920, 5943, 5967, 5990, 6014, 6037, 6060, 6084, 6107, 6130, 6154, 6177, 6200, 6223, 6247,
    6270, 6293, 6316, 6339, 6363, 6386, 6409, 6432, 6455, 6478, 6501, 6524, 6547, 6570, 6593, 6616,
    6639, 6662, 6685, 6708, 6731, 6754, 6777, 6800, 6823, 6846, 6868, 6891, 6914, 6937, 6960, 6982,
    7005, 7028, 7050, 7073, 7096, 7118, 7141, 7164, 7186, 7209, 7231, 7254, 7276, 7299, 7321, 7344,
    7366, 7389, 7411, 7434, 7456, 7478, 7501, 7523, 7545, 7568, 7590, 7612, 7635, 7657, 7679, 7701,
    7723, 7746, 7768, 7790, 7812, 7834, 7856, 7878, 7900, 7922, 7944, 7966, 7988, 8010, 8032, 8054,
    8076, 8098, 8119, 8141, 8163, 8185, 8207, 8228, 8250, 8272, 8293, 8315, 8337, 8358, 8380, 8401,
    8423, 8445, 8466, 8488, 8509, 8531, 8552, 8573, 8595, 8616, 8638, 8659, 8680, 8702, 8723, 8744,
    8765, 8787, 8808, 8829, 8850, 8871, 8892, 8914, 8935, 8956, 8977, 8998, 9019, 9040, 9061, 9082,
    9102, 9123, 9144, 9165, 9186, 9207, 9227, 9248, 9269, 9290, 9310, 9331, 9352, 9372, 9393, 9413,
    9434, 9455, 9475, 9496, 9516, 9537, 9557, 9577, 9598, 9618, 9638, 9659, 9679, 9699, 9720, 9740,
    9760, 9780, 9800, 9820, 9841, 9861, 9881, 9901, 9921, 9941, 9961, 9981, 10001, 10020, 10040, 10060,
    10080, 10100, 10120, 10139, 10159, 10179, 10198, 10218, 10238, 10257, 10277, 10296, 10316, 10336, 10355, 10374,
    10394, 10413, 10433, 10452, 10471, 10491, 10510, 10529, 10549, 10568, 10587, 10606, 10625, 10644, 10663, 10683,
    10702, 10721, 10740, 10759, 10778, 10796, 10815, 10834, 10853, 10872, 10891, 10909, 10928, 10947, 10966, 10984,
    11003, 11021, 11040, 11059, 11077, 11096, 11114, 11133, 11151, 11169, 11188, 11206, 11224, 11243, 11261, 11279,
    11297, 11316, 11334, 11352, 11370, 11388, 11406, 11424, 11442, 11460, 11478, 11496, 11514, 11532, 11550, 11567,
    11585, 11603, 11621, 11638, 11656, 11674, 11691, 11709, 11727, 11744, 11762, 11779, 11797, 11814, 11831, 11849,
    11866, 11883, 11901, 11918, 11935, 11952, 11970, 11987, 12004, 12021, 12038, 12055, 12072, 12089, 12106, 12123,
    12140, 12157, 12173, 12190, 12207, 12224, 12240, 12257, 12274, 12290, 12307, 12324, 12340, 12357, 12373, 12390,
    12406, 12423, 12439, 12455, 12472, 12488, 12504, 12520, 12537, 12553, 12569, 12585, 12601, 12617, 12633, 12649,
    12665, 12681, 12697, 12713, 12729, 12744, 12760, 12776, 12792, 12807, 12823, 12839, 12854, 12870, 12885, 12901,
    12916, 12932, 12947, 12963, 12978, 12993, 13008, 13024, 13039, 13054, 13069, 13085, 13100, 13115, 13130, 13145,
    13160, 13175, 13190, 13205, 13219, 13234, 13249, 13264, 13279, 13293, 13308, 13323, 13337, 13352, 13366, 13381,
    13395, 13410, 13424, 13439, 13453, 13467, 13482, 13496, 13510, 13524, 13538, 13553, 13567, 13581, 13595, 13609,
    13623, 13637, 13651, 13665, 13678, 13692, 13706, 13720, 13733, 13747, 13761, 13774, 13788, 13802, 13815, 13829,
    13842, 13856, 13869, 13882, 13896, 13909, 13922, 13935, 13949, 13962, 13975, 13988, 14001, 14014, 14027, 14040,
    14053, 14066, 14079, 14092, 14104, 14117, 14130, 14143, 14155, 14168, 14181, 14193, 14206, 14218, 14231, 14243,
    14256, 14268, 14280, 14293, 14305, 14317, 14329, 14341, 14354, 14366, 14378, 14390, 14402, 14414, 14426, 14438,
    14449, 14461, 14473, 14485, 14497, 14508, 14520, 14531, 14543, 14555, 14566, 14578, 14589, 14601, 14612, 14623,
    14635, 14646, 14657, 14668, 14680, 14691, 14702, 14713, 14724, 14735, 14746, 14757, 14768, 14779, 14789, 14800,
    14811, 14822, 14832, 14843, 14854, 14864, 14875, 14885, 14896, 14906, 14917, 14927, 14937, 14948, 14958, 14968,
    14978, 14989, 14999, 15009, 15019, 15029, 15039, 15049, 15059, 15069, 15078, 15088, 15098, 15108, 15118, 15127,
    15137, 15146, 15156, 15166, 15175, 15184, 15194, 15203, 15213, 15222, 15231, 15240, 15250, 15259, 15268, 15277,
    15286, 15295, 15304, 15313, 15322, 15331, 15340, 15349, 15357, 15366, 15375, 15383, 15392, 15401, 15409, 15418,
    15426, 15435, 15443, 15451, 15460, 15468, 15476, 15485, 15493, 15501, 15509, 15517, 15525, 15533, 15541, 15549,
    15557, 15565, 15573, 15581, 15588, 15596, 15604, 15611, 15619, 15627, 15634, 15642, 15649, 15656, 15664, 15671,
    15679, 15686, 15693, 15700, 15707, 15715, 15722, 15729, 15736, 15743, 15750, 15757, 15763, 15770, 15777, 15784,
    15791, 15797, 15804, 15810, 15817, 15824, 15830, 15837, 15843, 15849, 15856, 15862, 15868, 15875, 15881, 15887,
    15893, 15899, 15905, 15911, 15917, 15923, 15929, 15935, 15941, 15946, 15952, 15958, 15964, 15969, 15975, 15980,
    15986, 15991, 15997, 16002, 16008, 16013, 16018, 16024, 16029, 16034, 16039, 16044, 16049, 16054, 16059, 16064,
    16069, 16074, 16079, 16084, 16088, 16093, 16098, 16103, 16107, 16112, 16116, 16121, 16125, 16130, 16134, 16138,
    16143, 16147, 16151, 16156, 16160, 16164, 16168, 16172, 16176, 16180, 16184, 16188, 16192, 16195, 16199, 16203,
    16207, 16210, 16214, 16218, 16221, 16225, 16228, 16232, 16235, 16238, 16242, 16245, 16248, 16251, 16255, 16258,
    16261, 16264, 16267, 16270, 16273, 16276, 16279, 16281, 16284, 16287, 16290, 16292, 16295, 16298, 16300, 16303,
    16305, 16308, 16310, 16312, 16315, 16317, 16319, 16321, 16324, 16326, 16328, 16330, 16332, 16334, 16336, 16338,
    16340, 16341, 16343, 16345, 16347, 16348, 16350, 16352, 16353, 16355, 16356, 16358, 16359, 16360, 16362, 16363,
    16364, 16365, 16367, 16368, 16369, 16370, 16371, 16372, 16373, 16374, 16375, 16375, 16376, 16377, 16378, 16378,
    16379, 16380, 16380, 16381, 16381, 16382, 16382, 16382, 16383, 16383, 16383, 16384, 16384, 16384, 16384, 16384,
    16384,
    };

int R3D::Sin(int aAngle)
    {
    int a = aAngle & 4095;
    if (a <= 1024) return KSin[a];
    if (a <= 2048) return KSin[2048 - a];
    if (a <= 3072) return -KSin[a - 2048];
    return -KSin[4096 - a];
    }

// 1.13: the polygon list, 16-byte aligned (SPoly's corners each in a cache line)
static char* NewAligned(int aBytes, char*& aMem)
    {
    aMem = new char[aBytes + 15];
    if (!aMem) return 0;
    return (char*) (((unsigned long) aMem + 15) & ~(unsigned long) 15);
    }

R3D::R3D() : iFrame(0), iW(0), iH(0), iStride(0), iFocal(400), iCx(0), iCy(0),
    iCamX(0), iCamY(0), iCamZ(0), iYawS(0), iYawC(16384), iPitchS(0), iPitchC(16384), iWnSet(false),
    iFogStart(1 << 30), iFogEnd(1 << 30), iFogColor(0), iPersp(false), iCount(0), iMax(R3_MAXPOLY), iCap(0),
    iMV(0), iMVCap(0), iMXyz(0), iMBase(0), iMT(0), iMTCap(0), iMeshTexN(0)
    {
    iSpanMode = 1;
    iGeoMode = 1;
    iMeshMode = 1;
    iPolys = (SPoly*) NewAligned(R3_MAXPOLY * (int) sizeof(SPoly), iPolyMem);
    if (iPolys) iCap = R3_MAXPOLY;
    iBucket = new int[R3_BUCKETS * 2];
    iLeft = new int[R3_MAXH * 16];
    iRecip = new int[R3_RECIP];
    if (iRecip)
        {
        iRecip[0] = 1 << 30;
        for (int i = 1; i < R3_RECIP; i++) iRecip[i] = (1 << 30) / i;
        }
    }

R3D::~R3D()
    {
    delete[] iPolyMem;
    delete[] iBucket;
    delete[] iLeft;
    delete[] iRecip;
    delete[] iMV;
    delete[] iMT;
    }

bool R3D::Begin(R3Pix* aFrame, int aW, int aH, int aStride, int aFocal)
    {
    if (!iPolys || !iBucket || !iLeft || !iRecip || aH > R3_MAXH) return false;
    iFrame = aFrame; iW = aW; iH = aH; iStride = aStride; iFocal = aFocal;
    iCx = aW / 2; iCy = aH / 2;
    iWnSet = false;
    iCount = 0;
    iMeshTexN = 0;
    for (int i = 0; i < R3_BUCKETS * 2; i++) iBucket[i] = -1;
    R3Stats z = { 0, 0, 0, 0, 0, 0, 0 };
    iStats = z;
    return true;
    }

void R3D::SetMax(int aMax)
    {
    if (aMax < 1) aMax = 1;
    // 1.10: the list sized to the most asked for (between frames only: nothing collected yet); when
    // there's no RAM for a bigger one, the old one stays and the most is what it holds
    if (aMax != iCap && iCount == 0)
        {
        char* mem;
        SPoly* p = (SPoly*) NewAligned(aMax * (int) sizeof(SPoly), mem);
        if (p) { delete[] iPolyMem; iPolyMem = mem; iPolys = p; iCap = aMax; }
        }
    iMax = aMax < iCap ? aMax : iCap;
    }

void R3D::SetCamera(int aX, int aY, int aZ, int aYaw, int aPitch)
    {
    iCamX = aX; iCamY = aY; iCamZ = aZ;
    iYawS = Sin(aYaw); iYawC = Cos(aYaw);
    iPitchS = Sin(aPitch); iPitchC = Cos(aPitch);
    iWnSet = false;
    }

bool R3D::BoxVisible(int aX0, int aY0, int aZ0, int aX1, int aY1, int aZ1) const
    {
    // the corners into camera space as r3_verts does (64 bits: a far box is fine too), 2 units bigger
    int behind = 0, left = 0, right = 0, top = 0, bottom = 0;
    const long long F = iFocal, R = iW - iCx + 4, L = iCx + 4, T = iCy + 4, B = iH - iCy + 4;
    for (int k = 0; k < 8; k++)
        {
        long long dx = (long long) ((k & 1) ? aX1 + 2 : aX0 - 2) - iCamX;
        long long dy = (long long) ((k & 2) ? aY1 + 2 : aY0 - 2) - iCamY;
        long long dz = (long long) ((k & 4) ? aZ1 + 2 : aZ0 - 2) - iCamZ;
        long long zr = (dx * iYawS + dz * iYawC) >> 14;
        long long z = (dy * iPitchS + zr * iPitchC) >> 14;
        long long x = (dx * iYawC - dz * iYawS) >> 14;
        long long y = (dy * iPitchC - zr * iPitchS) >> 14;
        if (z < 4) behind++;
        // the four sides' planes through the camera (pixels: cx + F x / z, cy - F y / z), 4 px wider
        if (F * x > R * (z + 2)) right++;
        if (-F * x > L * (z + 2)) left++;
        if (F * y > T * (z + 2)) top++;
        if (-F * y > B * (z + 2)) bottom++;
        }
    return behind < 8 && left < 8 && right < 8 && top < 8 && bottom < 8;
    }

// 1.17: the view's 4 sides and the near plane as planes in world space (inward unit normals * 4096);
// a box wholly outside one (with 4 m to spare) isn't visible. The planes as BoxVisible's
// (8 px wider): a corner outside one is projected off the screen (r3_tris drops its triangles), behind it.
static int VPlaneLen(int aA, int aB)
    {
    unsigned int n = (unsigned int) (aA * aA + aB * aB), r = 0, bit = 1u << 30;
    while (bit > n) bit >>= 2;
    while (bit) { if (n >= r + bit) { n -= r + bit; r = (r >> 1) + bit; } else r >>= 1; bit >>= 2; }
    return r > 0 ? (int) r : 1;
    }

// a box (its middle dx, dy, dz from the camera, half sizes hx, hy, hz) against the planes in aMask
// (bit p: plane p; the box's parent was wholly inside the others): -1 wholly outside one, else the
// planes it straddles (0: wholly inside all, by the margin)
static int VBoxClass(int dx, int dy, int dz, int hx, int hy, int hz, const int (*wn)[6], int aMask)
    {
    if ((unsigned int) (dx + 32767) > 65534u || (unsigned int) (dy + 32767) > 65534u || (unsigned int) (dz + 32767) > 65534u) return aMask;
    // 4 m and 1/256 of the distance to spare (the normals' rounding)
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, az = dz < 0 ? -dz : dz;
    int m = (64 + ((ax + ay + az) >> 8)) * 4096, st = 0;
    for (int p = 0; p < 5; p++)
        {
        if (!(aMask & (1 << p))) continue;
        const int* n = wn[p];
        int c = dx * n[0] + dy * n[1] + dz * n[2];
        int e = hx * n[3] + hy * n[4] + hz * n[5];
        if (p == 4) c -= 4 * 4096;                  // the near plane at z = 4
        if (c + e < -m) return -1;
        if (c - e <= m) st |= 1 << p;
        }
    return st;
    }

int R3D::ChunksVisible(const int (*aS)[4], const int* aHy, int aOx, int aOy, int aOz, unsigned char* aVis)
    {
    if (!iWnSet)
        {
        // camera space normals (x right, y up, z forward): right (-F, 0, R), left (F, 0, L), top (0, -F, T),
        // bottom (0, F, B), near (0, 0, 1) at z = 4
        int F = iFocal, R = iW - iCx + 8, L = iCx + 8, T = iCy + 8, B = iH - iCy + 8;
        int cn[5][3] = { { -F, 0, R }, { F, 0, L }, { 0, -F, T }, { 0, F, B }, { 0, 0, 1 } };
        for (int p = 0; p < 5; p++)
            {
            int a = cn[p][0] + cn[p][1], b = cn[p][2];            // one of x, y is 0
            int len = p < 4 ? VPlaneLen(a, b) : 1;
            int nx = cn[p][0] * 4096 / len, ny = cn[p][1] * 4096 / len, nz = cn[p][2] * 4096 / len;
            int k = (nz * iPitchC - ny * iPitchS) >> 14;
            int* w = iWn[p];
            w[0] = (nx * iYawC + k * iYawS) >> 14;
            w[2] = (k * iYawC - nx * iYawS) >> 14;
            w[1] = (ny * iPitchC + nz * iPitchS) >> 14;
            for (int a2 = 0; a2 < 3; a2++) w[3 + a2] = w[a2] < 0 ? -w[a2] : w[a2];
            }
        iWnSet = true;
        }
    const int (*wn)[6] = iWn;
    int bx = aOx - iCamX, by = aOy - iCamY, bz = aOz - iCamZ;
    // the levels: 64 chunks (8 x 8), then 16 (4 x 4: 2 x 2 chunks each) at 64.., the whole at 80; each
    // tested against the planes its parent straddles only
    int cls[81];
    for (int i = 80; i >= 0; i--)
        {
        const int* s = aS[i];
        int pm = i == 80 ? 31 : cls[i >= 64 ? 80 : 64 + ((i & 7) >> 1) + (i >> 4) * 4];
        if (pm <= 0 || s[3] < 0) { cls[i] = s[3] < 0 ? -1 : pm; continue; }
        cls[i] = VBoxClass(s[0] + bx, s[1] + by, s[2] + bz, s[3] & 0xffff, aHy[i], (unsigned int) s[3] >> 16, wn, pm);
        }
    int vis = 0;
    for (int i = 0; i < 64; i++) { aVis[i] = (unsigned char) (cls[i] >= 0); vis += aVis[i]; }
    return vis;
    }

void R3D::SetFog(int aStart, int aEnd, R3Pix aColor)
    {
    iFogStart = aStart; iFogEnd = aEnd > aStart ? aEnd : aStart + 1; iFogColor = aColor;
    }

R3Pix R3D::Fog(R3Pix aColor, int aDepth) const
    {
    if (aDepth <= iFogStart) return aColor;
    int f = aDepth >= iFogEnd ? 16 : (aDepth - iFogStart) * 16 / (iFogEnd - iFogStart);
    int r = (aColor >> 11) & 31, g = (aColor >> 5) & 63, b = aColor & 31;
    int fr = (iFogColor >> 11) & 31, fg = (iFogColor >> 5) & 63, fb = iFogColor & 31;
    r += (fr - r) * f / 16; g += (fg - g) * f / 16; b += (fb - b) * f / 16;
    return (R3Pix) ((r << 11) | (g << 5) | b);
    }

void R3D::Background(R3Pix aSky, R3Pix aGround)
    {
    // the horizon: where the camera's level plane meets the screen
    int hy;
    if (iPitchC <= 0) hy = iPitchS > 0 ? iH : 0;
    else hy = iCy + iFocal * iPitchS / iPitchC;
    if (hy < 0) hy = 0;
    if (hy > iH) hy = iH;
    for (int y = 0; y < iH; y++) fm_fill16(iFrame + y * iStride, y < hy ? aSky : aGround, iW);
    }

// aLayer in the colour's place would be nicer; the ground is told apart by aTwoSided == false and
// a zero y on every vertex: it is drawn before everything else (see End)
void R3D::Poly(const R3Vtx* aV, int aN, const R3Tex* aTex, R3Pix aColor, bool aTwoSided)
    {
    iStats.polys++;
    if (iCount >= iMax || aN < 3 || aN > 4) return;
    // into camera space (0.75: tightened for the phone's GCC 2.9: one unsigned compare per range
    // check, no copies when nothing is clipped, no divisions or % for 3 and 4 corners: the same
    // results, ~30 % fewer instructions)
    SPv c[4];
    // 0.75: in assembly (r3dgeo.c): beyond ~1.9 km on an axis the 32-bit maths would overflow (such
    // polygons are fogged out anyway): -1
    int cam[9];
    cam[0] = iCamX; cam[1] = iCamY; cam[2] = iCamZ; cam[3] = iYawC; cam[4] = iYawS; cam[5] = iPitchC; cam[6] = iPitchS;
    cam[7] = 30000; cam[8] = 60000;
    int xf = iGeoMode ? r3_xform((const int*) aV, aN, (int*) c, cam) : r3_xform_c((const int*) aV, aN, (int*) c, cam);
    if (xf < 0) { iStats.culled++; return; }
    bool ground = xf < 256;
    int inFront = xf & 255;
    if (inFront == 0) { iStats.culled++; return; }
    // the near plane
    SPv clip[8];
    const SPv* o = c;
    int n = aN;
    if (inFront < aN)
        {
        iStats.clipped++;
        n = 0;
        for (int i = 0; i < aN; i++)
            {
            const SPv& a = c[i];
            const SPv& b = c[i + 1 < aN ? i + 1 : 0];
            bool ain = a.z >= R3_NEAR, bin = b.z >= R3_NEAR;
            if (ain) clip[n++] = a;
            if (ain != bin)
                {
                int t = ((R3_NEAR - a.z) << 12) / (b.z - a.z);         // 0..4096
                SPv& m = clip[n++];
                m.x = a.x + (((b.x - a.x) * t) >> 12);
                m.y = a.y + (((b.y - a.y) * t) >> 12);
                m.z = R3_NEAR;
                m.u = a.u + (int) (((long long) (b.u - a.u) * t) >> 12);
                m.v = a.v + (int) (((long long) (b.v - a.v) * t) >> 12);
                }
            }
        o = clip;
        }
    // onto the screen
    SPoly& p = iPolys[iCount];
    int minx = 1 << 30, maxx = -(1 << 30), miny = 1 << 30, maxy = -(1 << 30), zsum = 0;
    const int focal = iFocal, cx = iCx, cy = iCy;
    // 0.75: in assembly (r3dgeo.c) unless a corner is 512 m or more away (no reciprocal for it)
    R3ProjPrm prm;
    int box[5];
    prm.focal = focal; prm.cx = cx; prm.cy = cy; prm.recip = iRecip;
    if ((iGeoMode ? r3_project((const int*) o, n, p.sx, &prm, box) : r3_project_c((const int*) o, n, p.sx, &prm, box)) == 0)
        {
        minx = box[0]; maxx = box[1]; miny = box[2]; maxy = box[3]; zsum = box[4];
        }
    else for (int i = 0; i < n; i++)
        {
        // screen coordinates are kept within +-16000 (the edge slopes are 16.16)
        // 0.59: one reciprocal (a table) and two multiplies instead of two divisions
        int z = o[i].z;
        int inv = z < R3_RECIP ? iRecip[z] : (1 << 30) / z;
        int sx = cx + (int) (((long long) (focal * o[i].x) * inv) >> 30);
        int sy = cy - (int) (((long long) (focal * o[i].y) * inv) >> 30);
        if (sx < -16000) sx = -16000; else if (sx > 16000) sx = 16000;
        if (sy < -16000) sy = -16000; else if (sy > 16000) sy = 16000;
        p.sx[i] = sx; p.sy[i] = sy; p.sz[i] = z;
        p.su[i] = o[i].u; p.sv[i] = o[i].v;
        if (sx < minx) minx = sx;
        if (sx > maxx) maxx = sx;
        if (sy < miny) miny = sy;
        if (sy > maxy) maxy = sy;
        zsum += z;
        }
    if (maxx < 0 || minx >= iW || maxy < 0 || miny >= iH || maxy == miny) { iStats.culled++; return; }
    // the back face: screen y grows down, so a polygon counter-clockwise from the front is
    // clockwise on the screen
    int area = p.sx[n - 1] * p.sy[0] - p.sx[0] * p.sy[n - 1];
    for (int i = 0; i + 1 < n; i++) area += p.sx[i] * p.sy[i + 1] - p.sx[i + 1] * p.sy[i];
    if (area == 0 || (area > 0 && !aTwoSided)) { iStats.culled++; return; }
    p.n = n;
    p.down = area > 0 ? 1 : 0;      // 0.59: which side the downward edges are (1: the right)
    // zsum > 0 (every z >= R3_NEAR): / 4 is >> 2, / 3 a multiply (exact for all 32-bit values)
    p.depth = n == 4 ? zsum >> 2 : n == 3 ? (int) (((unsigned long long) (unsigned int) zsum * 0xAAAAAAABu) >> 33) : zsum / n;
    if (aTex) { p.texCopy = *aTex; p.tex = &p.texCopy; p.color = aColor; }
    else { p.tex = 0; p.color = Fog(aColor, p.depth); }
    // far to near: a bucket per depth range (the ground's buckets come first)
    int b = p.depth >> 4;
    if (b >= R3_BUCKETS) b = R3_BUCKETS - 1;
    b = R3_BUCKETS - 1 - b;
    if (!ground) b += R3_BUCKETS;
    p.next = iBucket[b];
    iBucket[b] = iCount;
    iCount++;
    }

int R3D::Bytes() const
    {
    return (int) sizeof(SPoly) * iCap + R3_BUCKETS * 2 * 4 + R3_MAXH * 16 * 4 + R3_RECIP * 4 + iMVCap * 8 + iMTCap * 4;
    }

bool R3D::MeshBegin(const short* aXyz, int aN, int aBase, int aOx, int aOy, int aOz, const unsigned short* aChV, const unsigned char* aGroups, int aNg)
    {
    if (aN > iMVCap)
        {
        delete[] iMV;
        iMVCap = (aN + 1023) & ~1023;
        iMV = new short[iMVCap * 4];
        if (!iMV) { iMVCap = 0; return false; }
        }
    iMXyz = aXyz; iMBase = aBase; iMOx = aOx; iMOy = aOy; iMOz = aOz;
    int cam[9];
    // 1.13: from the mesh's origin: dx = (x16 + ox) - camX = x16 - (camX - ox); y16 - (base - oy) is y - base
    cam[0] = iCamX - aOx; cam[1] = iCamY; cam[2] = iCamZ - aOz; cam[3] = iYawC; cam[4] = iYawS; cam[5] = iPitchC; cam[6] = iPitchS;
    cam[7] = 30000; cam[8] = 60000;
    R3ProjPrm prm;
    prm.focal = iFocal; prm.cx = iCx; prm.cy = iCy; prm.recip = iRecip;
    if (!aChV) { if (iGeoMode) r3_verts(aXyz, aN, aBase - aOy, iMV, cam, &prm); else r3_verts_c(aXyz, aN, aBase - aOy, iMV, cam, &prm); }
    else
        // 1.17: the corner groups wanted, adjacent ones in one go (the others' corners aren't read)
        for (int h = 0; h < aNg; )
            {
            if (!aGroups[h]) { h++; continue; }
            int e = h;
            while (e < aNg && aGroups[e]) e++;
            int a = aChV[h], n = aChV[e] - a;
            if (n > 0)
                {
                if (iGeoMode) r3_verts(aXyz + a * 3, n, aBase - aOy, iMV + a * 4, cam, &prm);
                else r3_verts_c(aXyz + a * 3, n, aBase - aOy, iMV + a * 4, cam, &prm);
                }
            h = e;
            }
    return true;
    }

int R3D::MeshTris(const unsigned short* aIdx, const unsigned short* aUv, const R3Pix* aShade, int aNf, int aUvShift,
    const R3Tex* aTex, const int* aSkip, int aNsk, const unsigned char* aFch, const unsigned char* aVis)
    {
    if (iCount >= iMax || aNf <= 0) return 0;
    if (aNf > iMTCap)
        {
        delete[] iMT;
        iMTCap = (aNf + 1023) & ~1023;
        iMT = new int[iMTCap];
        if (!iMT) { iMTCap = 0; return 0; }
        }
    // which ones: in assembly (r3dgeo.c)
    R3TriPrm tp;
    tp.idx = aIdx; tp.nf = aNf; tp.mv = iMV; tp.xyz = iMXyz; tp.skip = aSkip; tp.nsk = aNsk;
    tp.w = iW; tp.h = iH; tp.out = iMT; tp.skipped = 0; tp.fch = aFch; tp.vis = aVis;
    int kept = iGeoMode ? r3_tris(&tp) : r3_tris_c(&tp);
    int submitted = aNf - tp.skipped, slow = 0;
    const int shl = aUvShift < 0 ? 1 : 0, shr = aUvShift > 0 ? aUvShift : 0;
    const short* mv = iMV;
    const short* xyz = iMXyz;
    int i = 0;
    // 1.07: the polygons of the triangles all on the screen built in assembly (r3dgeo.c: r3_build),
    // a run at a time (it stops at one for Poly, and when the renderer is full)
    R3BuildPrm bp;
    bp.list = iMT; bp.kept = kept; bp.idx = aIdx; bp.uv = aUv; bp.shade = aShade; bp.mv = iMV;
    // 1.13: the polygons point at one copy of the texture (kept until End), or each has its own
    const R3Tex* shared = 0;
    if (aTex && iMeshTexN < KMeshTex) { iMeshTex[iMeshTexN] = *aTex; shared = &iMeshTex[iMeshTexN++]; }
    bp.polys = (char*) iPolys; bp.max = iMax; bp.bucket = iBucket; bp.tex = (const int*) shared;
    bp.shl = 16 + shr - shl; bp.shr = shl; bp.third = 0xAAAAAAABu;     // 1.16: r3_build takes these (the uv as words)
#ifdef FM_ASM
    // the layout r3_build writes (SPoly: n, down, tex, next, depth, color, texCopy, sx..sv; 32-bit pointers)
    typedef char KPolyLayout[sizeof(SPoly) == 208 && (int) &((SPoly*) 0)->down == 4 && (int) &((SPoly*) 0)->tex == 8
        && (int) &((SPoly*) 0)->next == 12 && (int) &((SPoly*) 0)->depth == 16 && (int) &((SPoly*) 0)->color == 20
        && (int) &((SPoly*) 0)->sx == 48 && (int) &((SPoly*) 0)->sv == 176 ? 1 : -1];
    (void) sizeof(KPolyLayout);
#endif
    for (; i < kept && iCount < iMax; i++)
        {
        if (shared && iGeoMode && iMT[i] >= 0)
            {
            bp.i = i; bp.count = iCount;
            r3_build(&bp);
            i = bp.i; iCount = bp.count;
            if (i >= kept || iCount >= iMax) break;
            }
        int f = iMT[i];
        const unsigned short* t = aUv + (f & 0xffffff) * 6;
        if (f < 0)
            {
            // a corner behind the camera (clipped), out of range or 512 m away: Poly
            f &= 0xffffff;
            slow++;
            const unsigned short* q = aIdx + f * 3;
            R3Vtx v[3];
            const short* p = xyz + q[0] * 3;
            v[0].x = p[0] + iMOx; v[0].y = p[1] + iMOy - iMBase; v[0].z = p[2] + iMOz; v[0].u = (t[0] << shl) >> shr; v[0].v = (t[1] << shl) >> shr;
            p = xyz + q[1] * 3;
            v[1].x = p[0] + iMOx; v[1].y = p[1] + iMOy - iMBase; v[1].z = p[2] + iMOz; v[1].u = (t[2] << shl) >> shr; v[1].v = (t[3] << shl) >> shr;
            p = xyz + q[2] * 3;
            v[2].x = p[0] + iMOx; v[2].y = p[1] + iMOy - iMBase; v[2].z = p[2] + iMOz; v[2].u = (t[4] << shl) >> shr; v[2].v = (t[5] << shl) >> shr;
            Poly(v, 3, aTex, aShade[f]);
            continue;
            }
        const unsigned short* q = aIdx + f * 3;
        const short* a = mv + q[0] * 4;
        const short* b = mv + q[1] * 4;
        const short* c = mv + q[2] * 4;
        SPoly& p = iPolys[iCount];
        p.sx[0] = a[0]; p.sx[1] = b[0]; p.sx[2] = c[0];
        p.sy[0] = a[1]; p.sy[1] = b[1]; p.sy[2] = c[1];
        int za = a[2], zb = b[2], zc = c[2];
        p.sz[0] = za; p.sz[1] = zb; p.sz[2] = zc;
        p.su[0] = (t[0] << shl) >> shr; p.sv[0] = (t[1] << shl) >> shr;
        p.su[1] = (t[2] << shl) >> shr; p.sv[1] = (t[3] << shl) >> shr;
        p.su[2] = (t[4] << shl) >> shr; p.sv[2] = (t[5] << shl) >> shr;
        p.n = 3;
        p.down = 0;
        unsigned int zsum = za + zb + zc;
        int depth = (int) (((unsigned long long) zsum * 0xAAAAAAABu) >> 33);
        p.depth = depth;
        if (shared) { p.tex = shared; p.color = aShade[f]; }
        else if (aTex) { p.texCopy = *aTex; p.tex = &p.texCopy; p.color = aShade[f]; }
        else { p.tex = 0; p.color = Fog(aShade[f], depth); }
        int bk = depth >> 4;
        if (bk >= R3_BUCKETS) bk = R3_BUCKETS - 1;
        bk = R3_BUCKETS - 1 - bk;
        if ((a[3] | b[3] | c[3]) & 2) bk += R3_BUCKETS;
        p.next = iBucket[bk];
        iBucket[bk] = iCount;
        iCount++;
        }
    // the counts as Poly's (those it got counted themselves)
    iStats.polys += submitted - slow;
    iStats.culled += submitted - kept;
    return submitted;
    }

void R3D::End()
    {
#ifdef FM_ASM
    if (!iPersp && iSpanMode == 1)
        {
        // 1.13: the loop in assembly (r3dgeo.c: r3_draw), the textured polygons drawn there; it stops at
        // the others (flat, or too big for r3_raster_tex): Raster
        R3DrawPrm dp;
        dp.polys = (const char*) iPolys; dp.bucket = iBucket; dp.b = 0; dp.i = iBucket[0]; dp.nb = R3_BUCKETS * 2; dp.drawn = 0;
        dp.rp.L = iLeft; dp.rp.h = iH; dp.rp.recip = iRecip; dp.rp.wmax = iW - 1; dp.rp.frame = iFrame;
        dp.rp.strideBytes = iStride * 2; dp.rp.pixels = 0;
        typedef char KPolyLayout2[(int) &((SPoly*) 0)->down == 4 && (int) &((SPoly*) 0)->tex == 8 && sizeof(R3Tex) == 16
            && (int) &((R3Tex*) 0)->stride == 12 ? 1 : -1];
        (void) sizeof(KPolyLayout2);
        while (iGeoMode ? r3_draw(&dp) : r3_draw_c(&dp))
            {
            Raster(iPolys[dp.i]);
            dp.drawn++;
            dp.i = iPolys[dp.i].next;
            }
        iStats.drawn += dp.drawn;
        iStats.pixels += dp.rp.pixels;
        return;
        }
#endif
    for (int b = 0; b < R3_BUCKETS * 2; b++)
        for (int i = iBucket[b]; i >= 0; i = iPolys[i].next)
            {
            Raster(iPolys[i]);
            iStats.drawn++;
            }
    }

// One convex polygon, flat or with an affine texture (0.59): the edges give only x per scanline (the
// left and right ends); u and v are planes over the screen, their gradients worked out once a polygon,
// so a span starts with two multiplies and no division. Spans: fastmem's fill, or the assembly
// textured span (r3dspan.c).
void R3D::Raster(const SPoly& aP)
    {
    if (iPersp && aP.tex) { RasterPersp(aP); return; }
    const R3Tex* t0 = aP.tex;
    if (t0 && iSpanMode == 1 && t0->stride == (1 << t0->wShift) && r3_span_ok(t0->wShift, t0->hShift))
        {
        // 1.12: a textured polygon at once in assembly (r3dgeo.c: the rows' ends, the texture's planes, the rows)
        R3RasterPrm rp;
        rp.sx = aP.sx; rp.n = aP.n; rp.down = aP.down; rp.L = iLeft; rp.h = iH; rp.recip = iRecip;
        rp.wmax = iW - 1; rp.frame = iFrame; rp.strideBytes = iStride * 2; rp.tex = t0->pix;
        rp.wShift = t0->wShift; rp.hShift = t0->hShift; rp.pixels = 0;
        int rr = iGeoMode ? r3_raster_tex(&rp) : r3_raster_tex_c(&rp);
        iStats.pixels += rp.pixels;
        if (rr == 0) return;
        }
    int* L = iLeft;                                 // per scanline: [0] left x, [1] right x (16.16)
    int y0 = 1 << 30, y1 = -(1 << 30);
    // 1.07: the rows' ends in assembly (r3dgeo.c: r3_edges) unless the polygon is 8192 rows or taller
    R3EdgePrm ep;
    ep.sx = aP.sx; ep.n = aP.n; ep.down = aP.down; ep.L = L; ep.h = iH; ep.recip = iRecip;
    int er = iGeoMode ? r3_edges(&ep) : r3_edges_c(&ep);
    if (er == -2) return;
    if (er >= 0) { y0 = er & 0xffff; y1 = er >> 16; }
    else
    {
    for (int i = 0; i < aP.n; i++)
        {
        if (aP.sy[i] < y0) y0 = aP.sy[i];
        if (aP.sy[i] > y1) y1 = aP.sy[i];
        }
    if (y0 < 0) y0 = 0;
    if (y1 > iH - 1) y1 = iH - 1;
    if (y0 > y1) return;
    // convex: the edges going down are all on one side, the others on the other (by the winding),
    // so each scanline's end is just stored (no compares, no clearing)
    for (int i = 0; i < aP.n; i++)
        {
        int j = i + 1 < aP.n ? i + 1 : 0;
        int ya = aP.sy[i], yb = aP.sy[j], xa = aP.sx[i], xb = aP.sx[j];
        if (ya == yb) continue;
        int side = aP.down;
        if (ya > yb) { int t = ya; ya = yb; yb = t; t = xa; xa = xb; xb = t; side ^= 1; }
        int dy = yb - ya;
        int rec = dy < R3_RECIP ? iRecip[dy] : (1 << 30) / dy;
        int x = xa << 16, dx = (int) (((long long) (xb - xa) * rec) >> 14);
        int ys = ya, ye = yb;
        if (ys < 0) { x += dx * -ys; ys = 0; }
        if (ye > iH - 1) ye = iH - 1;
        int* r = L + ys * 2 + side;
        for (int y = ys; y <= ye; y++, r += 2, x += dx) *r = x;
        }
    }
    const R3Tex* t = aP.tex;
    R3Span sp;
    unsigned int ux = 0, uy = 0, vx = 0, vy = 0, u00 = 0, v00 = 0;
    bool fast = false, span4 = false;
    if (t)
        {
        // the texture's planes from the first three corners: u = u00 + ux * x + uy * y (16.16)
        // 0.92: in assembly (r3dgeo.c) unless twice the area is 8192 px or more
        unsigned int g[6];
        int gr = iGeoMode ? r3_texgrad(aP.sx, iRecip, g) : r3_texgrad_c(aP.sx, iRecip, g);
        if (gr == 1) return;
        if (gr == 0) { ux = g[0]; uy = g[1]; vx = g[2]; vy = g[3]; u00 = g[4]; v00 = g[5]; }
        else
        {
        int x0 = aP.sx[0], yA = aP.sy[0], x1 = aP.sx[1] - x0, y1d = aP.sy[1] - yA, x2 = aP.sx[2] - x0, y2d = aP.sy[2] - yA;
        int u1 = aP.su[1] - aP.su[0], u2 = aP.su[2] - aP.su[0], v1 = aP.sv[1] - aP.sv[0], v2 = aP.sv[2] - aP.sv[0];
        long long d = (long long) x1 * y2d - (long long) x2 * y1d;
        if (d == 0) return;
        bool neg = d < 0;
        if (neg) d = -d;
        // 2^30 / d (d < 2^31: the screen coordinates are kept within +-16000)
        int inv = d < R3_RECIP ? iRecip[(int) d] : (int) ((1 << 30) / (int) d);
        if (neg) inv = -inv;
        // texels * 256 per pixel, times 2^30 / d, to 16.16: >> 22
        ux = (unsigned int) (((long long) u1 * y2d - (long long) u2 * y1d) * inv >> 22);
        uy = (unsigned int) (((long long) u2 * x1 - (long long) u1 * x2) * inv >> 22);
        vx = (unsigned int) (((long long) v1 * y2d - (long long) v2 * y1d) * inv >> 22);
        vy = (unsigned int) (((long long) v2 * x1 - (long long) v1 * x2) * inv >> 22);
        u00 = ((unsigned int) aP.su[0] << 8) - ux * x0 - uy * yA;
        v00 = ((unsigned int) aP.sv[0] << 8) - vx * x0 - vy * yA;
        }
        sp.tex = t->pix; sp.du = (int) ux; sp.dv = (int) vx;
        sp.shift = t->wShift; sp.wmask = (1 << t->wShift) - 1; sp.hmask = (1 << t->hShift) - 1;
        fast = t->stride == (1 << t->wShift);
        span4 = iSpanMode == 1 && r3_span_ok(t->wShift, t->hShift);
        }
    if (span4 && fast && iSpanMode == 1)
        {
        // 0.75: the rows in assembly too (r3dspan.c)
        R3Rows rw;
        int s = t->wShift - 3;
        rw.L = L + y0 * 2; rw.row = iFrame + y0 * iStride; rw.stride = iStride * 2; rw.rows = y1 - y0 + 1;
        rw.wmax = iW - 1;
        rw.urow = u00 + uy * (unsigned int) y0; rw.uy = uy;
        rw.vrow = (v00 + vy * (unsigned int) y0) << s; rw.vy = vy << s;
        rw.tex = t->pix; rw.du = ux; rw.dv = vx << s;
        rw.umask = (unsigned int) ((1 << t->wShift) - 1) << 1;
        rw.vmask = (unsigned int) ((1 << t->hShift) - 1) << (t->wShift + 1);
        rw.pixels = 0;
        r3_rows_tex4(&rw);
        iStats.pixels += rw.pixels;
        return;
        }
    int* r = L + y0 * 2;
    R3Pix* row = iFrame + y0 * iStride;
    for (int y = y0; y <= y1; y++, r += 2, row += iStride)
        {
        if (r[0] > r[1]) continue;
        int xa = (r[0] + 0xffff) >> 16, xb = r[1] >> 16;   // pixel centres inside
        if (xa < 0) xa = 0;
        if (xb > iW - 1) xb = iW - 1;
        if (xa > xb) continue;
        int cnt = xb - xa + 1;
        iStats.pixels += cnt;
        if (!t) { fm_fill16(row + xa, aP.color, cnt); continue; }
        unsigned int u = u00 + ux * xa + uy * y, v = v00 + vx * xa + vy * y;
        if (fast)
            {
            sp.dst = row + xa; sp.count = cnt; sp.u = (int) u; sp.v = (int) v;
            if (span4) r3_span_tex4(&sp); else r3_span_tex(&sp);
            continue;
            }
        int wm = (1 << t->wShift) - 1, hm = (1 << t->hShift) - 1, st = t->stride;
        R3Pix* p = row + xa;
        while (cnt--)
            {
            *p++ = t->pix[((((int) v) >> 16) & hm) * st + ((((int) u) >> 16) & wm)];
            u += ux; v += vx;
            }
        }
    }

// Perspective-correct textures (the 0.40 rasteriser): per scanline the leftmost and rightmost edge
// crossings with x, u, v (16.16), 1/z, u/z, v/z, then the spans (exact u, v every 16 pixels: 64-bit
// divisions, ~4x the affine time on the phone; 0.59: not used by the app any more).
void R3D::RasterPersp(const SPoly& aP)
    {
    // per scanline: [0] xl [1] ul [2] vl [3] wl [4..5] uzl [6..7] vzl, [8..15] the same for the right
    int* L = iLeft;
    int y0 = 1 << 30, y1 = -(1 << 30);
    for (int i = 0; i < aP.n; i++)
        {
        if (aP.sy[i] < y0) y0 = aP.sy[i];
        if (aP.sy[i] > y1) y1 = aP.sy[i];
        }
    if (y0 < 0) y0 = 0;
    if (y1 > iH - 1) y1 = iH - 1;
    if (y0 > y1) return;
    for (int y = y0; y <= y1; y++) { L[y * 16] = 0x7fffffff; L[y * 16 + 8] = -0x7fffffff; }
    bool persp = iPersp && aP.tex;
    for (int i = 0; i < aP.n; i++)
        {
        int j = (i + 1) % aP.n;
        int ya = aP.sy[i], yb = aP.sy[j];
        if (ya == yb) continue;
        int a = i, b = j;
        if (ya > yb) { a = j; b = i; int t = ya; ya = yb; yb = t; }
        int dy = yb - ya;
        int rec = dy < R3_RECIP ? iRecip[dy] : (1 << 30) / dy;          // 0.59: no divisions
        int x = aP.sx[a] << 16, dx = (int) (((long long) (aP.sx[b] - aP.sx[a]) * rec) >> 14);
        int u = aP.su[a] << 8, du = (int) (((long long) (aP.su[b] - aP.su[a]) * rec) >> 22);
        int v = aP.sv[a] << 8, dv = (int) (((long long) (aP.sv[b] - aP.sv[a]) * rec) >> 22);
        // perspective: 1/z as 2^30/z, u/z and v/z as 64-bit
        long long w = 0, dw = 0, uz = 0, duz = 0, vz = 0, dvz = 0;
        if (persp)
            {
            long long wa = (1LL << 30) / aP.sz[a], wb = (1LL << 30) / aP.sz[b];
            long long ua = (long long) aP.su[a] * wa, ub = (long long) aP.su[b] * wb;
            long long va = (long long) aP.sv[a] * wa, vb = (long long) aP.sv[b] * wb;
            w = wa; dw = (wb - wa) / dy; uz = ua; duz = (ub - ua) / dy; vz = va; dvz = (vb - va) / dy;
            }
        int ys = ya, ye = yb;
        if (ys < 0)
            {
            int s = -ys;
            x += dx * s; u += du * s; v += dv * s;
            if (persp) { w += dw * s; uz += duz * s; vz += dvz * s; }
            ys = 0;
            }
        if (ye > iH - 1) ye = iH - 1;
        for (int y = ys; y <= ye; y++)
            {
            int* r = L + y * 16;
            if (x < r[0])
                {
                r[0] = x; r[1] = u; r[2] = v;
                if (persp) { r[3] = (int) w; *(long long*) (r + 4) = uz; *(long long*) (r + 6) = vz; }
                }
            if (x > r[8])
                {
                r[8] = x; r[9] = u; r[10] = v;
                if (persp) { r[11] = (int) w; *(long long*) (r + 12) = uz; *(long long*) (r + 14) = vz; }
                }
            x += dx; u += du; v += dv;
            if (persp) { w += dw; uz += duz; vz += dvz; }
            }
        }
    const R3Tex* t = aP.tex;
    int wm = 0, hm = 0, ws = 0, st = 0;
    if (t) { wm = (1 << t->wShift) - 1; hm = (1 << t->hShift) - 1; ws = t->wShift; st = t->stride; }
    for (int y = y0; y <= y1; y++)
        {
        int* r = L + y * 16;
        if (r[0] > r[8]) continue;
        int xa = (r[0] + 0xffff) >> 16, xb = r[8] >> 16;      // pixel centres inside
        int span = (r[8] - r[0]) >> 16;
        int cl = xa < 0 ? -xa : 0;
        if (xa < 0) xa = 0;
        if (xb > iW - 1) xb = iW - 1;
        if (xa > xb) continue;
        R3Pix* p = iFrame + y * iStride + xa;
        int cnt = xb - xa + 1;
        iStats.pixels += cnt;
        if (!t)
            {
            fm_fill16(p, aP.color, cnt);            // 0.59: fastmem (STM, 16 pixels at a time)
            continue;
            }
        if (!persp)
            {
            int rs = span <= 0 ? 0 : span < R3_RECIP ? iRecip[span] : (1 << 30) / span;
            int du = (int) (((long long) (r[9] - r[1]) * rs) >> 30), dv = (int) (((long long) (r[10] - r[2]) * rs) >> 30);
            int u = r[1] + du * cl, v = r[2] + dv * cl;
            if (st == (1 << ws))
                {
                // 0.59: the assembly span (r3dspan.c): 10 instructions a pixel
                R3Span sp;
                sp.dst = p; sp.count = cnt; sp.tex = t->pix; sp.u = u; sp.v = v; sp.du = du; sp.dv = dv;
                sp.shift = ws; sp.wmask = wm; sp.hmask = hm;
                r3_span_tex(&sp);
                continue;
                }
            const R3Pix* tx = t->pix;
            while (cnt--)
                {
                *p++ = tx[(((v >> 16) & hm) * st) + ((u >> 16) & wm)];
                u += du; v += dv;
                }
            continue;
            }
        // perspective-correct: exact u, v every 16 pixels, linear between
        long long w = r[3], uz = *(long long*) (r + 4), vz = *(long long*) (r + 6);
        long long dw = 0, duz = 0, dvz = 0;
        if (span > 0)
            {
            dw = ((long long) r[11] - r[3]) / span;
            duz = (*(long long*) (r + 12) - uz) / span;
            dvz = (*(long long*) (r + 14) - vz) / span;
            }
        w += dw * cl; uz += duz * cl; vz += dvz * cl;
        const R3Pix* tx = t->pix;
        int u0 = w > 0 ? (int) ((uz << 8) / w) : 0, v0 = w > 0 ? (int) ((vz << 8) / w) : 0;
        while (cnt > 0)
            {
            int k = cnt > 16 ? 16 : cnt;
            w += dw * k; uz += duz * k; vz += dvz * k;
            int u1 = w > 0 ? (int) ((uz << 8) / w) : u0, v1 = w > 0 ? (int) ((vz << 8) / w) : v0;
            int du = (u1 - u0) / k, dv = (v1 - v0) / k;
            int u = u0, v = v0;
            for (int i = 0; i < k; i++)
                {
                *p++ = tx[(((v >> 16) & hm) * st) + ((u >> 16) & wm)];
                u += du; v += dv;
                }
            u0 = u1; v0 = v1;
            cnt -= k;
            }
        }
    }
