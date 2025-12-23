#include "poker.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

const char* mark_Aart[MARK_MAX] = { //トランプの記号
    "♥",   //ASKII_HEART = 0
    "♣",   //ASKII_CLUB = 1
    "◆",   //ASKII_DIAMOND = 2
    "♠",   //ASKII_SPADE = 3
};

const char* number_Aart[14] = { //トランプ用数字
    "0 ",
    "1 ", "2 ", "3 ", "4 ", "5 ",
    "6 ", "7 ", "8 ", "9 ", "10",
    "J ", "Q ", "K "
};

const char* illust_Aart[MARK_MAX][ILLUST_MAX] = { //トランプ面のイラスト素材
    {"|         |",
     "|    ❤︎    |",
     "|  ❤︎   ❤︎  |",
     "|  ❤︎ ❤︎ ❤︎  |",
     " _________ ",
     "|_________|",},
    {"|         |",
     "|    ♣︎    |",
     "|  ♣︎   ♣︎  |",
     "|  ♣︎ ♣︎ ♣︎  |",
     " _________ ",
     "|_________|",},
    {"|         |",
     "|    ♦︎    |",
     "|  ♦︎   ♦︎  |",
     "|  ♦︎ ♦︎ ♦︎  |",
     " _________ ",
     "|_________|",},
    {"|         |",
     "|    ♠︎    |",
     "|  ♠︎   ♠︎  |",
     "|  ♠︎ ♠︎ ♠︎  |",
     " _________ ",
     "|_________|",},
};

int illust_low_list[14] = { //トランプ面のイラスト定義
    0, //0
    100, //1
    1010, //2
    10101, //3
    20002, //4
    20102, //5
    20202, //6
    20212, //7
    21212, //8
    22122, //9
    22222, //10
    22322, //11
    23232, //12
    23332  //13
};

const char* poker_hand_art[POKERHAND_MAX] = { //役リスト, 下に行くほど強い
    "NoHand",
    "OnePair",
    "TwoPair",
    "ThreeCard",
    "Straight",
    "Flush",
    "FullHouse",
    "FourCard",
    "StraightFlush",
    "RoyalStraightFlush",
};

TRUMP all_trumps[NUM_ALL_TRUMPS]; //52枚全てのトランプ
HAND player1_hand[5]; //1Pの手札
HAND player2_hand[5]; //2Pの手札
COUNTHAND p1p2_counter[3];
int top = 0; //山札の一番上を指す

static void ResetCounter(COUNTHAND *counter){
    for(int i = 0; i < 4; i++){
        counter->suit[i] = 0;
    }
    for(int i = 0; i <= 13; i++){
        counter->number[i] = 0;
    }
    counter->poker_hand = NoHand;
    counter->point_poker_hand = 0;
    counter->tiebreak_len = 0;
    for(int i = 0; i < 5; i++){
        counter->tiebreak[i] = 0;
    }
}

static int CardRank(int number){
    return (number == 1) ? 14 : number;
}

static void SortDesc(int *array, int len){
    for(int i = 0; i < len - 1; i++){
        for(int j = i + 1; j < len; j++){
            if(array[i] < array[j]){
                int tmp = array[i];
                array[i] = array[j];
                array[j] = tmp;
            }
        }
    }
}

static bool IsStraightFromCounts(const int rank_counts[15], int *straight_high){
    if(rank_counts[14] && rank_counts[5] && rank_counts[4] && rank_counts[3] && rank_counts[2]){
        *straight_high = 5;
        return true;
    }
    for(int high = 14; high >= 5; high--){
        bool ok = true;
        for(int r = high; r > high - 5; r--){
            if(rank_counts[r] != 1){
                ok = false;
                break;
            }
        }
        if(ok){
            *straight_high = high;
            return true;
        }
    }
    return false;
}

void InitTrumps(){ //トランプの配列に52枚を書き込む
    for(int i = 0; i < 4; i++){
        for(int j = 0; j < 13; j++){
            all_trumps[i*13 + j].suit = i;
            all_trumps[i*13 + j].number = j+1;
        }
    }
}

void ShuffleTrumps(){ //52枚のトランプをシャッフルする
    for(int i = NUM_ALL_TRUMPS - 1; i > 0; i--){
        int j = rand() % (i + 1);
        TRUMP tmp = all_trumps[j]; //tempはバッファ
        all_trumps[j] = all_trumps[i];
        all_trumps[i] = tmp;
    }
}

void InitHand(){ //手札を配る関数
    for(int i = 0; i < 5; i++){
        player1_hand[i].suit = all_trumps[top].suit;
        player1_hand[i].number = all_trumps[top].number;
        player1_hand[i].is_exchange = false;
        top++;
        player2_hand[i].suit = all_trumps[top].suit;
        player2_hand[i].number = all_trumps[top].number;
        player2_hand[i].is_exchange = false;
        top++;
    }
}

void InitWithSeed(unsigned int seed){ //初期化関数
    top = 0;
    srand(seed);
    InitTrumps();
    ShuffleTrumps();
    InitHand();
}

void Init(){ //初期化関数
    InitWithSeed((unsigned int)time(NULL));
}

void ExchangeHand(){ //手札を交換する関数
    for(int i = 0; i < 5; i++){
        if(player1_hand[i].is_exchange == true){
            if(top >= NUM_ALL_TRUMPS){
                printf("山札終了\n");
                continue;
            }
            player1_hand[i].number = all_trumps[top].number;
            player1_hand[i].suit = all_trumps[top].suit;
            player1_hand[i].is_exchange = false;
            top++;
        }
        if(player2_hand[i].is_exchange == true){
            if(top >= NUM_ALL_TRUMPS){
                printf("山札終了\n");
                continue;
            }
            player2_hand[i].number = all_trumps[top].number;
            player2_hand[i].suit = all_trumps[top].suit;
            player2_hand[i].is_exchange = false;
            top++;
        }
    }
}

void Count(int no){ //手札の要素をカウント(擬似ソート)
    ResetCounter(&p1p2_counter[no]);
    p1p2_counter[no].number[0] = -1;

    if(no == 1){
        for(int i = 0; i < 5; i++){
            p1p2_counter[no].number[player1_hand[i].number]++;
            p1p2_counter[no].suit[player1_hand[i].suit]++;
        }
    }
    if(no == 2){
        for(int i = 0; i < 5; i++){
            p1p2_counter[no].number[player2_hand[i].number]++;
            p1p2_counter[no].suit[player2_hand[i].suit]++;
        }
    }
}

void JudgeHand(int no){
    if(no == 1){
        EvaluateHand(player1_hand, &p1p2_counter[no]);
    }
    if(no == 2){
        EvaluateHand(player2_hand, &p1p2_counter[no]);
    }
}

void EvaluateHand(const HAND hand[5], COUNTHAND *out){
    int rank_counts[15] = {0};
    int suit_counts[4] = {0};
    int ranks[5];
    int suits[5];

    ResetCounter(out);
    for(int i = 0; i < 5; i++){
        ranks[i] = CardRank(hand[i].number);
        suits[i] = hand[i].suit;
        rank_counts[ranks[i]]++;
        suit_counts[suits[i]]++;
    }

    bool is_flush = false;
    for(int i = 0; i < 4; i++){
        if(suit_counts[i] == 5){
            is_flush = true;
            break;
        }
    }

    int straight_high = 0;
    bool is_straight = IsStraightFromCounts(rank_counts, &straight_high);

    int four = 0;
    int three = 0;
    int pairs[2] = {0, 0};
    int pair_count = 0;
    int kickers[5] = {0};
    int kicker_count = 0;

    for(int r = 14; r >= 2; r--){
        if(rank_counts[r] == 4){
            four = r;
        }else if(rank_counts[r] == 3){
            three = r;
        }else if(rank_counts[r] == 2){
            if(pair_count < 2){
                pairs[pair_count++] = r;
            }
        }else if(rank_counts[r] == 1){
            kickers[kicker_count++] = r;
        }
    }

    int sorted[5];
    for(int i = 0; i < 5; i++){
        sorted[i] = ranks[i];
    }
    SortDesc(sorted, 5);

    if(is_flush && is_straight){
        if(straight_high == 14 && rank_counts[10] && rank_counts[11] && rank_counts[12] && rank_counts[13]){
            out->poker_hand = RoyalStraightFlush;
            out->tiebreak[0] = 14;
            out->tiebreak_len = 1;
        }else{
            out->poker_hand = StraightFlush;
            out->tiebreak[0] = straight_high;
            out->tiebreak_len = 1;
        }
    }else if(four){
        out->poker_hand = FourCard;
        out->tiebreak[0] = four;
        out->tiebreak[1] = kickers[0];
        out->tiebreak_len = 2;
    }else if(three && pair_count == 1){
        out->poker_hand = FullHouse;
        out->tiebreak[0] = three;
        out->tiebreak[1] = pairs[0];
        out->tiebreak_len = 2;
    }else if(is_flush){
        out->poker_hand = Flush;
        for(int i = 0; i < 5; i++){
            out->tiebreak[i] = sorted[i];
        }
        out->tiebreak_len = 5;
    }else if(is_straight){
        out->poker_hand = Straight;
        out->tiebreak[0] = straight_high;
        out->tiebreak_len = 1;
    }else if(three){
        out->poker_hand = ThreeCard;
        out->tiebreak[0] = three;
        for(int i = 0; i < kicker_count; i++){
            out->tiebreak[i + 1] = kickers[i];
        }
        out->tiebreak_len = 1 + kicker_count;
    }else if(pair_count == 2){
        out->poker_hand = TwoPair;
        int high_pair = (pairs[0] > pairs[1]) ? pairs[0] : pairs[1];
        int low_pair = (pairs[0] > pairs[1]) ? pairs[1] : pairs[0];
        out->tiebreak[0] = high_pair;
        out->tiebreak[1] = low_pair;
        out->tiebreak[2] = kickers[0];
        out->tiebreak_len = 3;
    }else if(pair_count == 1){
        out->poker_hand = OnePair;
        out->tiebreak[0] = pairs[0];
        for(int i = 0; i < kicker_count; i++){
            out->tiebreak[i + 1] = kickers[i];
        }
        out->tiebreak_len = 1 + kicker_count;
    }else{
        out->poker_hand = NoHand;
        for(int i = 0; i < 5; i++){
            out->tiebreak[i] = sorted[i];
        }
        out->tiebreak_len = 5;
    }

    if(out->tiebreak_len > 0){
        out->point_poker_hand = out->tiebreak[0];
    }
}

int CompareHands(const COUNTHAND *a, const COUNTHAND *b){
    if(a->poker_hand > b->poker_hand){
        return 1;
    }
    if(a->poker_hand < b->poker_hand){
        return -1;
    }

    int max_len = (a->tiebreak_len > b->tiebreak_len) ? a->tiebreak_len : b->tiebreak_len;
    for(int i = 0; i < max_len; i++){
        int left = (i < a->tiebreak_len) ? a->tiebreak[i] : 0;
        int right = (i < b->tiebreak_len) ? b->tiebreak[i] : 0;
        if(left > right){
            return 1;
        }
        if(left < right){
            return -1;
        }
    }
    return 0;
}

void JudgeWinner(){
    int cmp = CompareHands(&p1p2_counter[1], &p1p2_counter[2]);
    if(cmp > 0){
        printf("––––– Winner: PLAYER 1 –––––\n\n");
    }
    else if(cmp < 0){
        printf("––––– Winner: PLAYER 2 –––––\n\n");
    }
    else{ //p1p2_counter[1].poker_hand == p1p2_counter[2].poker_hand
        printf("––––– DRAW –––––\n\n");
    }
}


//以下print関係の関数–––––––––––––––––––––––––––––––––––––––––––––––––––––
void PrintTrumpCard(int mark, int num){ //ASKIIでトランプカードを表示
    printf("%s\n", illust_Aart[0][ILLUST_UPPER]); //共通の最上段をプリント
    printf("|%s%s      |\n", mark_Aart[mark], number_Aart[num]); //マークと数字の入った行をプリント

    int illust_low = illust_low_list[num]; //イラスト定義から取り出す
    for(int i = 0; i < 5; i++){ //中央のイラストをプリント
        int low = illust_low % 10;
        printf("%s\n", illust_Aart[mark][low]);
        illust_low = illust_low / 10;
    }

    printf("%s\n", illust_Aart[0][ILLUST_LOWER]); //(共通の)最下段をプリント
}

void PrintAllTrumps(){ //デバッグ用, 52枚のトランプをprint
    printf("All trumps\n");
    for(int i = 0; i < NUM_ALL_TRUMPS; i++){
        if(i == top){
            printf("top→ ");
        }
        printf("%s:%s, ", number_Aart[all_trumps[i].number], mark_Aart[all_trumps[i].suit]);
    }
    printf("\n");
}

void PrintHand(int no){ //デバッグ用, 引数のプレイヤーの手札をprint
    printf("P%d_hand\n", no);
    for(int i = 0; i < 5; i++){
        if(no == 1){
            printf("%s:%s\n", number_Aart[player1_hand[i].number], mark_Aart[player1_hand[i].suit]);
        }
        if(no == 2){
            printf("%s:%s\n", number_Aart[player2_hand[i].number], mark_Aart[player2_hand[i].suit]);
        } 
    }
}

void PrintCounter(int no){ //デバッグ用, 引数のプレイヤーの手札カウンターをprint
    printf("P%d_counter\n", no);
    for(int i = 0; i < 4; i++){
        printf("%d,", p1p2_counter[no].suit[i]);
    }
    printf("\n");
    for(int i = 0; i <= 13; i++){
        printf("%d,", p1p2_counter[no].number[i]);
    }
    printf("\n");
}

void PrintPokerHand(int no){ //デバッグ用, 役を表示
    printf("P%d_poker_hand\n", no);
    printf("hand; %s\n", poker_hand_art[p1p2_counter[no].poker_hand]);
    printf("point; %d\n", p1p2_counter[no].point_poker_hand);
    if(p1p2_counter[no].tiebreak_len > 0){
        printf("tiebreak:");
        for(int i = 0; i < p1p2_counter[no].tiebreak_len; i++){
            printf(" %d", p1p2_counter[no].tiebreak[i]);
        }
        printf("\n");
    }
    printf("\n");
}

#ifndef POKER_TEST
int main(){
    Init();

    PrintHand(1);
    Count(1);
    PrintCounter(1);
    JudgeHand(1);
    PrintPokerHand(1);
    
    PrintHand(2);
    Count(2);
    PrintCounter(2);
    JudgeHand(2);
    PrintPokerHand(2);

    JudgeWinner();

    return 0;
}
#endif
