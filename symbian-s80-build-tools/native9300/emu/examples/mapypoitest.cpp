// Maps 9300 (1.15): the Mapy.com places module on the PC: the captured tiles, a detail and reviews.
// g++ -O2 -I../inc mapypoitest.cpp ../src/mapypoi.cpp ../src/poi.cpp ../src/json.cpp ../src/route.cpp -o mapypoitest
// mapypoitest tile <file> <tx> <ty> | detail <file> | reviews <file>
#include "mapypoi.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char buf[1 << 21];
int main(int argc, char** argv)
    {
    if (argc < 3) return 1;
    if (!strcmp(argv[1], "xml"))
        {
        char x[3000];
        long long id = atoll(argv[2]);
        char s[10]; long long sid;
        if (MpSource(id, s, sizeof s, sid)) printf("%s %lld\n", s, sid); else printf("unknown source\n");
        int l = MpDetailXml(x, sizeof x, "firm", 12846125, 172552366, 495906336, "cs");
        printf("%d %s\n", l, x);
        l = MpDetailXml(x, sizeof x, 0, 0, 172552366, -495906336, "cs");
        printf("%d %s\n", l, x);
        return 0;
        }
    FILE* f = fopen(argv[2], "rb");
    if (!f) return 2;
    int n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    if (!strcmp(argv[1], "tile"))
        {
        static Poi p[2000];
        static char text[1 << 20];
        int tl;
        int c = MpTileParse(buf, n, atoi(argv[3]), atoi(argv[4]), p, 2000, text, sizeof text, tl);
        printf("%d places, %d B of text\n", c, tl);
        for (int i = 0; i < c && i < (argc > 5 ? atoi(argv[5]) : 1000); i++)
            {
            char name[200], id[30];
            PoiField(text, p[i], EMpName, name, sizeof name);
            PoiField(text, p[i], EMpId, id, sizeof id);
            printf("%d %d %s %s | %s\n", p[i].px, p[i].py, PoiKindAt(p[i].kind).tag, id, name);
            }
        }
    else if (!strcmp(argv[1], "detail"))
        {
        static FNode nodes[4000];
        static char out[8000];
        char title[200], photo[300], src[20];
        long long sid;
        int l = MpDetailText(buf, n, nodes, 4000, out, sizeof out, title, sizeof title, photo, sizeof photo, src, sizeof src, sid);
        printf("%d: [%s] photo [%s] %s %lld\n%s\n", l, title, photo, src, sid, out);
        }
    else if (!strcmp(argv[1], "reviews"))
        {
        int cap = JsonNodeCount((const char*) buf, n);
        JNode* nodes = new JNode[cap];
        static char out[8000];
        int l = MpReviewsText((const char*) buf, n, nodes, cap, out, sizeof out);
        printf("%d:\n%s\n", l, out);
        }
    else if (!strcmp(argv[1], "xml"))
        {
        char x[3000];
        long long id = atoll(argv[2]);
        char s[10]; long long sid;
        if (MpSource(id, s, sizeof s, sid)) printf("%s %lld\n", s, sid); else printf("unknown source\n");
        int l = MpDetailXml(x, sizeof x, "firm", 12846125, 172552366, 495906336, "cs");
        printf("%d %s\n", l, x);
        l = MpDetailXml(x, sizeof x, 0, 0, 172552366, -495906336, "cs");
        printf("%d %s\n", l, x);
        }
    return 0;
    }
