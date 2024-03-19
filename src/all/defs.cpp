// (c)2024, Arthur van Hoff, Artfahrt Inc.
#include "util.h"

extern const char *short_name(int cs)
{
    switch (cs) {
    case CARD_NULL : return "N";
    case CARD_EMPTY: return "E";
    case CARD_FAIL: return "F";
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
    case 9: card_name = "J"; break;
    case 10: card_name = "Q"; break;
    case 11: card_name = "K"; break;
    case 12: card_name = "A"; break;
    default: card_name = "?"; break;
    }
    const char *suit_name;
    switch (SUIT(cs)) {
    case 0: suit_name = "C"; break;
    case 1: suit_name = "D"; break;
    case 2: suit_name = "H"; break;
    case 3: suit_name = "S"; break;
    default: suit_name = "?"; break;
    }
    static char short_name_buf[8];
    snprintf(short_name_buf, sizeof(short_name_buf), "%s%s", card_name, suit_name);
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
