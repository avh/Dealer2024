// (c)2024, Arthur van Hoff, Artfahrt Inc.
#include "deal.h"

// E:QT96.KT6.KQJT.Q5,A42.J2.98643.KJ4,J75.Q9543.7.A983,K83.A87.A52.T762
// N:...23456789TJQKA|..23456789TJQKA.|.23456789TJQKA..|23456789TJQKA...
// N:JQKA.QKA.QKA.QKA|R

char card2ch(int c)
{
    return (c < 0 || c >= SUITLEN) ? '?' : "23456789TJQKA"[c];
}

int ch2card(char c)
{
    const char *p = strchr("23456789TJQKA", c);
    return p ? p - "23456789TJQKA" : CARD_NULL;
}

char suit2ch(int s)
{
    return (s < 0 || s >= NSUITS) ? '?' : "CDHS"[s];
}

const char *suit2sym(int s)
{
    switch (s) {
    case 0: return "♣";
    case 1: return "♦";
    case 2: return "♥";
    case 3: return "♠";
    default: return "?";
    }
}
int ch2suit(char c)
{
    const char *p = strchr("CDHS", c);
    return p ? p - "CDHS" : CARD_NULL;
}

char player2ch(int p)
{
    return (p < 0 || p >= NPLAYERS) ? '?' : "NESW"[p];
}

int ch2player(char c)
{
    const char *p = strchr("NESW", c);
    return p ? p - "NESW" : -1;
}

int card2hcp(int c) 
{
    c = c % SUITLEN;
    return (c < 9) ? 0 : c - 8; 
}

extern const char *short_name(int cs)
{
    switch (cs) {
    case CARD_NULL : return "N";
    case CARD_EMPTY: return "E";
    case CARD_FAIL: return "F";
    }

    static char short_name_buf[8];
    snprintf(short_name_buf, sizeof(short_name_buf), "%c%s", card2ch(CARD(cs)), suit2sym(SUIT(cs)));
    return short_name_buf;
}

extern const char *full_name(int cs)
{
    switch (cs) {
    case CARD_NULL : return "No Card";
    case CARD_EMPTY: return "Empty Hopper";
    case CARD_FAIL: return "Card Failed";
    }
    const char *card_name;
    int c = CARD(cs);
    switch (CARD(cs)) {
    case 0: card_name = "2"; break;
    case 1: card_name = "3"; break;
    case 2: card_name = "4"; break;
    case 3: card_name = "5"; break;
    case 4: card_name = "6"; break;
    case 5: card_name = "7"; break;
    case 6: card_name = "8"; break;
    case 7: card_name = "9"; break;
    case 8: card_name = "10"; break;
    case 9: card_name = "Jack"; break;
    case 10: card_name = "Queen"; break;
    case 11: card_name = "King"; break;
    case 12: card_name = "Ace"; break;
    default: card_name = "Unknown"; break;
    }
    const char *suit_name;
    switch (SUIT(cs)) {
    case 0: suit_name = "Clubs"; break;
    case 1: suit_name = "Diamonds"; break;
    case 2: suit_name = "Hearts"; break;
    case 3: suit_name = "Spades"; break;
    default: suit_name = "Unknown"; break;
    }
    static char full_name_buf[32];
    snprintf(full_name_buf, sizeof(full_name_buf), "%s of %s", card_name, suit_name);
    return full_name_buf;
}

bool Deal::parse(const char *str)
{
    for (int i = 0 ; i < DECKLEN ; i++) {
        owner[i] = CARD_NULL;
    }
    for (int p = 0 ; p < NPLAYERS ; p++) {
        for (int c = 0 ; c < HANDSIZE ; c++) {
            cards[p][c] = CARD_NULL;
        }
    }
    
    const char *p = str;
    dealer = ch2player(*p++);
    if (dealer < 0) {
        dprintf("deal: invalid dealer '%str'", str);
        return false;
    }
    if (*p++ != ':') {
        dprintf("deal: colon expected '%str'", str);
        return false;
    }
    for (int j = 0, player = dealer ; j < NPLAYERS ; j++, player = (player + 1) % 4) {
        if (*p == 'R') {
            break;
        }
        for (int i = 0, suit = 3 ; i < HANDSIZE && suit >= 0 ; i++) {
            if (*p == '|' || *p == ',' || *p == ' ') {
                p++;
                break;
            }
            if (*p == 'R' || *p == '\0') {
                break;
            }
            for (; *p == '.' && suit > 0 ; suit--, p++); 
            int card =  ch2card(*p++);
            if (card < 0 || card >= SUITLEN) {
                dprintf("deal: invalid card '%str', card=%d, c='%c'", str, card, p[-1]);
                return false;
            }
            int cs = CARDSUIT(suit, card);
            if (owner[cs] != CARD_NULL) {
                dprintf("deal: duplicate card '%str', card=%d, c='%c'", str, card, p[-1]);
                return false;
            }
            dprintf("deal: player=%d, i=%d, suit=%d, card=%d, cs=%d", player, i, suit, card, cs);
            cards[player][i] = cs;
            owner[cs] = player;
        }
        if (*p == 'R' || *p == '\0') {
            break;
        }
    }
    // RANDOMIZE
    if (*p == 'R') {
        p++;
        for (int p = 0 ; p < NPLAYERS ; p++) {
            for (int c = 0 ; c < HANDSIZE ; c++) {
                if (cards[p][c] == CARD_NULL) {
                    int off = esp_random() % DECKLEN;
                    for (int i = 0 ; i < DECKLEN ; i++) {
                        if (owner[(off + i) % DECKLEN] == CARD_NULL) {
                            cards[p][c] = (off + i) % DECKLEN;
                            owner[(off + i) % DECKLEN] = p;
                            break;
                        }
                    }
                }
            }
        }
    }
    if (*p != '\0') {
        dprintf("deal: extra characters '%str'", str);
        return false;
    }
    // check assignments
    for (int p = 0 ; p < NPLAYERS ; p++) {
        for (int c = 0 ; c < HANDSIZE ; c++) {
            if (cards[p][c] == CARD_NULL) {
                dprintf("deal: missing card for player %d", p);
                return false;
            }
        }
    }
    for (int p = 0 ; p < NPLAYERS ; p++) {
        std::sort(&cards[p][0], &cards[p][HANDSIZE]);
    }
    return true;
}

void Deal::debug()
{
    for (int i = 0 ; i < NPLAYERS ; i++) {
        int hcp = 0;
        char buf[128];
        int n =  snprintf(buf, sizeof(buf), "%c:", player2ch(i));
        for (int j = 0 ; j < HANDSIZE ; j++) {
            n += snprintf(buf + n, sizeof(buf) - n, " %s", short_name(cards[i][j]));
            hcp += card2hcp(cards[i][j]);
        }
        dprintf("%s - %2d", buf, hcp);
    }
}