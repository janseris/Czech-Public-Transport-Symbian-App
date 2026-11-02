// Maps 9300: a small JSON parser for the Mapy.com and OSRM answers (search, routes). The whole
// text becomes an array of nodes (no allocation per value: a long route has tens of thousands of
// numbers); strings are undone only when asked for. Portable C++, tested on a PC (test/jsontest).

#include "json.h"

static int SkipWs(const char* t, int i, int n)
    {
    while (i < n && (t[i] == ' ' || t[i] == '\t' || t[i] == '\r' || t[i] == '\n')) i++;
    return i;
    }

int JsonNodeCount(const char* aText, int aLen)
    {
    // every value is preceded by '[', '{', ',' or ':' (or is the root); ':' only for members
    int n = 2;
    bool str = false;
    for (int i = 0; i < aLen; i++)
        {
        char c = aText[i];
        if (str) { if (c == '\\') i++; else if (c == '"') str = false; continue; }
        if (c == '"') str = true;
        else if (c == ',' || c == '[' || c == '{') n++;
        }
    return n;
    }

struct JParser
    {
    const char* t;
    int n, pos;
    JNode* nodes;
    int cap, used;
    };

static int Value(JParser& p, int depth);

static int NewNode(JParser& p, char aType)
    {
    if (p.used >= p.cap) return -1;
    JNode& j = p.nodes[p.used];
    j.type = aType;
    j.start = p.pos;
    j.len = 0;
    j.keyStart = -1;
    j.keyLen = 0;
    j.first = -1;
    j.next = -1;
    return p.used++;
    }

// a string starting at p.pos ('"'); its raw content start and length
static bool String(JParser& p, int& aStart, int& aLen)
    {
    if (p.pos >= p.n || p.t[p.pos] != '"') return false;
    int i = p.pos + 1;
    aStart = i;
    while (i < p.n && p.t[i] != '"') { if (p.t[i] == '\\') i++; i++; }
    if (i >= p.n) return false;
    aLen = i - aStart;
    p.pos = i + 1;
    return true;
    }

static int Value(JParser& p, int depth)
    {
    if (depth > 64) return -1;
    p.pos = SkipWs(p.t, p.pos, p.n);
    if (p.pos >= p.n) return -1;
    char c = p.t[p.pos];
    if (c == '{' || c == '[')
        {
        int me = NewNode(p, c == '{' ? 'o' : 'a');
        if (me < 0) return -1;
        p.pos++;
        int last = -1;
        char close = c == '{' ? '}' : ']';
        p.pos = SkipWs(p.t, p.pos, p.n);
        if (p.pos < p.n && p.t[p.pos] == close) { p.pos++; return me; }
        for (;;)
            {
            int ks = -1, kl = 0;
            if (c == '{')
                {
                p.pos = SkipWs(p.t, p.pos, p.n);
                if (!String(p, ks, kl)) return -1;
                p.pos = SkipWs(p.t, p.pos, p.n);
                if (p.pos >= p.n || p.t[p.pos] != ':') return -1;
                p.pos++;
                }
            int child = Value(p, depth + 1);
            if (child < 0) return -1;
            p.nodes[child].keyStart = ks;
            p.nodes[child].keyLen = kl;
            if (last < 0) p.nodes[me].first = child; else p.nodes[last].next = child;
            last = child;
            p.pos = SkipWs(p.t, p.pos, p.n);
            if (p.pos >= p.n) return -1;
            if (p.t[p.pos] == ',') { p.pos++; continue; }
            if (p.t[p.pos] == close) { p.pos++; return me; }
            return -1;
            }
        }
    if (c == '"')
        {
        int me = NewNode(p, 's');
        if (me < 0) return -1;
        int s, l;
        if (!String(p, s, l)) return -1;
        p.nodes[me].start = s;
        p.nodes[me].len = l;
        return me;
        }
    if (c == 't' || c == 'f' || c == 'n')
        {
        int me = NewNode(p, c == 'n' ? 'z' : c);
        if (me < 0) return -1;
        while (p.pos < p.n && p.t[p.pos] >= 'a' && p.t[p.pos] <= 'z') p.pos++;
        return me;
        }
    if (c == '-' || (c >= '0' && c <= '9'))
        {
        int me = NewNode(p, 'n');
        if (me < 0) return -1;
        int s = p.pos;
        while (p.pos < p.n)
            {
            char d = p.t[p.pos];
            if ((d >= '0' && d <= '9') || d == '-' || d == '+' || d == '.' || d == 'e' || d == 'E') p.pos++;
            else break;
            }
        p.nodes[me].start = s;
        p.nodes[me].len = p.pos - s;
        return me;
        }
    return -1;
    }

int JsonParse(const char* aText, int aLen, JNode* aNodes, int aCap)
    {
    JParser p;
    p.t = aText; p.n = aLen; p.pos = 0; p.nodes = aNodes; p.cap = aCap; p.used = 0;
    if (Value(p, 0) != 0) return -1;
    return p.used;
    }

static bool KeyIs(const char* t, const JNode& j, const char* aKey)
    {
    int i = 0;
    for (; i < j.keyLen; i++) if (aKey[i] == 0 || t[j.keyStart + i] != aKey[i]) return false;
    return aKey[i] == 0;
    }

int JsonGet(const char* aText, const JNode* aNodes, int aObj, const char* aKey)
    {
    if (aObj < 0 || aNodes[aObj].type != 'o') return -1;
    for (int c = aNodes[aObj].first; c >= 0; c = aNodes[c].next)
        if (KeyIs(aText, aNodes[c], aKey)) return c;
    return -1;
    }

int JsonCount(const JNode* aNodes, int aIdx)
    {
    if (aIdx < 0) return 0;
    int n = 0;
    for (int c = aNodes[aIdx].first; c >= 0; c = aNodes[c].next) n++;
    return n;
    }

double JsonNum(const char* aText, const JNode* aNodes, int aIdx)
    {
    if (aIdx < 0 || aNodes[aIdx].type != 'n') return 0;
    const char* s = aText + aNodes[aIdx].start;
    int n = aNodes[aIdx].len, i = 0;
    bool neg = false;
    if (i < n && (s[i] == '-' || s[i] == '+')) { neg = s[i] == '-'; i++; }
    double v = 0;
    while (i < n && s[i] >= '0' && s[i] <= '9') v = v * 10 + (s[i++] - '0');
    if (i < n && s[i] == '.')
        {
        i++;
        double f = 0.1;
        while (i < n && s[i] >= '0' && s[i] <= '9') { v += (s[i++] - '0') * f; f *= 0.1; }
        }
    if (i < n && (s[i] == 'e' || s[i] == 'E'))
        {
        i++;
        bool eneg = false;
        if (i < n && (s[i] == '-' || s[i] == '+')) { eneg = s[i] == '-'; i++; }
        int e = 0;
        while (i < n && s[i] >= '0' && s[i] <= '9') e = e * 10 + (s[i++] - '0');
        while (e-- > 0) v = eneg ? v / 10 : v * 10;
        }
    return neg ? -v : v;
    }

static int Hex(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 0; }

int JsonStr(const char* aText, const JNode* aNodes, int aIdx, char* aOut, int aCap)
    {
    int o = 0;
    if (aCap <= 0) return 0;
    if (aIdx >= 0 && aNodes[aIdx].type == 's')
        {
        const char* s = aText + aNodes[aIdx].start;
        int n = aNodes[aIdx].len;
        for (int i = 0; i < n && o < aCap - 4; i++)
            {
            char c = s[i];
            if (c != '\\' || i + 1 >= n) { aOut[o++] = c; continue; }
            c = s[++i];
            if (c == 'n') aOut[o++] = ' ';
            else if (c == 't') aOut[o++] = ' ';
            else if (c == 'r' || c == 'b' || c == 'f') {}
            else if (c == 'u' && i + 4 < n)
                {
                unsigned int u = (Hex(s[i + 1]) << 12) | (Hex(s[i + 2]) << 8) | (Hex(s[i + 3]) << 4) | Hex(s[i + 4]);
                i += 4;
                if (u < 0x80) aOut[o++] = (char) u;
                else if (u < 0x800) { aOut[o++] = (char) (0xc0 | (u >> 6)); aOut[o++] = (char) (0x80 | (u & 63)); }
                else { aOut[o++] = (char) (0xe0 | (u >> 12)); aOut[o++] = (char) (0x80 | ((u >> 6) & 63)); aOut[o++] = (char) (0x80 | (u & 63)); }
                }
            else aOut[o++] = c;
            }
        }
    aOut[o] = 0;
    return o;
    }
