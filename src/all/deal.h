// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#pragma once
#include "util.h"

#define NPLAYERS             4    
#define DECKLEN             52
#define NSUITS              4
#define SUITLEN             (DECKLEN / NSUITS)
#define HANDSIZE            (DECKLEN / NPLAYERS)

enum {NORTH, EAST, SOUTH, WEST};

class Deal {
  public:
    int dealer;
    unsigned char cards[NPLAYERS][HANDSIZE];
    unsigned char owner[DECKLEN];
  public:
    bool parse(const char *str);
    void debug();
};

extern const char *short_name(int cs);
extern const char *full_name(int cs);
extern char card2ch(int c);
extern int ch2card(char c);
extern char suit2ch(int s);
extern const char *suit2sym(int s);
extern int ch2suit(char c);
extern char player2ch(int p);
extern int ch2player(char c);
extern int card2hcp(int c);
