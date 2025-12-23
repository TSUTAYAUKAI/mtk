#ifndef POKER_H
#define POKER_H

#include <stdbool.h>

#define NUM_ALL_TRUMPS  52

enum { //トランプの記号
    HEART,
    CLUB,
    DIAMOND,
    SPADE,
    MARK_MAX
};

enum { //トランプ面のイラスト素材
    ILLUST_0,
    ILLUST_1,
    ILLUST_2,
    ILLUST_3,
    ILLUST_UPPER,
    ILLUST_LOWER,
    ILLUST_MAX
};

enum { //役リスト, 下に行くほど強い
    NoHand,             //0
    OnePair,            //1
    TwoPair,            //2
    ThreeCard,          //3
    Straight,           //4
    Flush,              //5
    FullHouse,          //6
    FourCard,           //7
    StraightFlush,      //8
    RoyalStraightFlush, //9
    POKERHAND_MAX
};

typedef struct { //トランプ型
    int suit;
    int number;
} TRUMP;

typedef struct { //手札型
    int suit;
    int number;
    bool is_exchange;
} HAND;

typedef struct {
    int suit[4];
    int number[13+1];
    int poker_hand;
    int point_poker_hand;
    int tiebreak[5];
    int tiebreak_len;
} COUNTHAND;

extern TRUMP all_trumps[NUM_ALL_TRUMPS];
extern HAND player1_hand[5];
extern HAND player2_hand[5];
extern COUNTHAND p1p2_counter[3];
extern int top;

void InitTrumps(void);
void ShuffleTrumps(void);
void InitHand(void);
void InitWithSeed(unsigned int seed);
void Init(void);
void ExchangeHand(void);
void Count(int no);
void JudgeHand(int no);
int CompareHands(const COUNTHAND *a, const COUNTHAND *b);
void JudgeWinner(void);
void PrintHand(int no);
void PrintPokerHand(int no);
void PrintTrumpCard(int mark, int num);
void PrintAllTrumps(void);
void EvaluateHand(const HAND hand[5], COUNTHAND *out);

#endif
