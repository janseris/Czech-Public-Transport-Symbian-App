// Maps 9300: a small JSON parser (see json.cpp). Portable C++: tested on a PC.

#ifndef JSON_H
#define JSON_H

// One value. Objects and arrays link their children (first, next); an object's members carry their
// key (keyStart, keyLen: the raw text between the quotes). Strings and numbers point into the text.
struct JNode
    {
    char type;                  // 'o' object, 'a' array, 's' string, 'n' number, 't' true, 'f' false, 'z' null
    int start, len;             // string: raw content (escapes not undone), number: its text
    int keyStart, keyLen;       // as a member of an object, -1 otherwise
    int first, next;            // children / siblings, -1 at the end
    };

// A upper bound of the nodes JsonParse needs for aText.
int JsonNodeCount(const char* aText, int aLen);

// Parses aText into aNodes (aCap of them). The number of nodes (node 0 is the root), or -1.
int JsonParse(const char* aText, int aLen, JNode* aNodes, int aCap);

// The member aKey of object aObj, or -1.
int JsonGet(const char* aText, const JNode* aNodes, int aObj, const char* aKey);

// The value as a number (0 if it isn't one).
double JsonNum(const char* aText, const JNode* aNodes, int aIdx);

// The string's content in UTF-8 (escapes undone, \uXXXX too), zero-terminated, at most aCap - 1 bytes.
int JsonStr(const char* aText, const JNode* aNodes, int aIdx, char* aOut, int aCap);

// The number of elements of an array or members of an object.
int JsonCount(const JNode* aNodes, int aIdx);

#endif
