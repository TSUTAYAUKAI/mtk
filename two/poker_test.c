#include "poker.h"

#include <stdio.h>
#include <stdlib.h>

static HAND MakeCard(int suit, int number){
    HAND card;
    card.suit = suit;
    card.number = number;
    card.is_exchange = false;
    return card;
}

static void BuildHand(HAND hand[5], const int suits[5], const int numbers[5]){
    for(int i = 0; i < 5; i++){
        hand[i] = MakeCard(suits[i], numbers[i]);
    }
}

static void ExpectHand(const char *name, const HAND hand[5], int expected_hand, int expected_high){
    COUNTHAND result;
    EvaluateHand(hand, &result);
    if(result.poker_hand != expected_hand){
        printf("FAIL %s: expected hand=%d got=%d\n", name, expected_hand, result.poker_hand);
        exit(1);
    }
    if(expected_high > 0 && result.tiebreak_len > 0 && result.tiebreak[0] != expected_high){
        printf("FAIL %s: expected high=%d got=%d\n", name, expected_high, result.tiebreak[0]);
        exit(1);
    }
}

static void ExpectCompare(const char *name, const HAND a[5], const HAND b[5], int expected_sign){
    COUNTHAND left;
    COUNTHAND right;
    EvaluateHand(a, &left);
    EvaluateHand(b, &right);
    int sign = CompareHands(&left, &right);
    if(sign != expected_sign){
        printf("FAIL %s: expected compare=%d got=%d\n", name, expected_sign, sign);
        exit(1);
    }
}

int main(void){
    HAND hand[5];
    HAND hand2[5];

    {
        int suits[5] = {SPADE, SPADE, SPADE, SPADE, SPADE};
        int numbers[5] = {10, 11, 12, 13, 1};
        BuildHand(hand, suits, numbers);
        ExpectHand("RoyalStraightFlush", hand, RoyalStraightFlush, 14);
    }

    {
        int suits[5] = {HEART, HEART, HEART, HEART, HEART};
        int numbers[5] = {5, 6, 7, 8, 9};
        BuildHand(hand, suits, numbers);
        ExpectHand("StraightFlush", hand, StraightFlush, 9);
    }

    {
        int suits[5] = {HEART, CLUB, DIAMOND, SPADE, HEART};
        int numbers[5] = {9, 9, 9, 9, 1};
        BuildHand(hand, suits, numbers);
        ExpectHand("FourCard", hand, FourCard, 9);
    }

    {
        int suits[5] = {HEART, CLUB, DIAMOND, SPADE, HEART};
        int numbers[5] = {3, 3, 3, 13, 13};
        BuildHand(hand, suits, numbers);
        ExpectHand("FullHouse", hand, FullHouse, 3);
    }

    {
        int suits[5] = {CLUB, CLUB, CLUB, CLUB, CLUB};
        int numbers[5] = {1, 10, 7, 4, 2};
        BuildHand(hand, suits, numbers);
        ExpectHand("Flush", hand, Flush, 14);
    }

    {
        int suits[5] = {HEART, CLUB, DIAMOND, SPADE, HEART};
        int numbers[5] = {1, 2, 3, 4, 5};
        BuildHand(hand, suits, numbers);
        ExpectHand("StraightAto5", hand, Straight, 5);
    }

    {
        int suits[5] = {HEART, CLUB, DIAMOND, SPADE, HEART};
        int numbers[5] = {7, 7, 7, 13, 2};
        BuildHand(hand, suits, numbers);
        ExpectHand("ThreeCard", hand, ThreeCard, 7);
    }

    {
        int suits[5] = {HEART, CLUB, DIAMOND, SPADE, HEART};
        int numbers[5] = {11, 11, 4, 4, 1};
        BuildHand(hand, suits, numbers);
        ExpectHand("TwoPair", hand, TwoPair, 11);
    }

    {
        int suits[5] = {HEART, CLUB, DIAMOND, SPADE, HEART};
        int numbers[5] = {12, 12, 1, 10, 3};
        BuildHand(hand, suits, numbers);
        ExpectHand("OnePair", hand, OnePair, 12);
    }

    {
        int suits[5] = {HEART, CLUB, DIAMOND, SPADE, HEART};
        int numbers[5] = {1, 13, 9, 6, 3};
        BuildHand(hand, suits, numbers);
        ExpectHand("NoHand", hand, NoHand, 14);
    }

    {
        int suits_a[5] = {HEART, CLUB, DIAMOND, SPADE, HEART};
        int numbers_a[5] = {10, 10, 5, 4, 2};
        int suits_b[5] = {HEART, CLUB, DIAMOND, SPADE, HEART};
        int numbers_b[5] = {9, 9, 13, 4, 2};
        BuildHand(hand, suits_a, numbers_a);
        BuildHand(hand2, suits_b, numbers_b);
        ExpectCompare("PairBeatsPair", hand, hand2, 1);
    }

    {
        int suits_a[5] = {HEART, CLUB, DIAMOND, SPADE, HEART};
        int numbers_a[5] = {6, 6, 6, 2, 1};
        int suits_b[5] = {HEART, CLUB, DIAMOND, SPADE, HEART};
        int numbers_b[5] = {5, 5, 5, 13, 12};
        BuildHand(hand, suits_a, numbers_a);
        BuildHand(hand2, suits_b, numbers_b);
        ExpectCompare("TripsBeatsTrips", hand, hand2, 1);
    }

    {
        int suits_a[5] = {HEART, HEART, HEART, HEART, HEART};
        int numbers_a[5] = {1, 10, 8, 4, 2};
        int suits_b[5] = {CLUB, CLUB, CLUB, CLUB, CLUB};
        int numbers_b[5] = {13, 10, 8, 4, 2};
        BuildHand(hand, suits_a, numbers_a);
        BuildHand(hand2, suits_b, numbers_b);
        ExpectCompare("FlushHighCard", hand, hand2, 1);
    }

    printf("PASS\n");
    return 0;
}
