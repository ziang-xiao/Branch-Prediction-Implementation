//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "NAME";
const char *studentID = "PID";
const char *email = "EMAIL";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = {"Static", "Gshare",
                         "Tournament", "Custom"};

int ghistoryBits; // Number of bits used for Global History
int lhistoryBits; // Number of bits used for Local History
int pcIndexBits;  // Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

uint32_t ghistory; // used to record global history
uint32_t gh_mask;  // used as global history mask;

//for gshare
uint8_t *gshareBHT; // used as Branch History Table

//for tournament
uint32_t *lPHT;      // used as Local History Table
uint8_t *lBHT;       // used as Local Counter
uint8_t *gBHT;       // used as Global Counter
uint8_t *selector;   // used as prediction selector
uint8_t lprediction; //used to record local prediction
uint8_t gprediction; //used to record global prediction

uint32_t pc_mask;
uint32_t lh_mask;

int customBits;
int trainLimits; // Number of entries in the percepton table
int customHeights; // Number of entries in the percepton table
int theta;
int **weights; //Perceptron weights on pc_size and heights
uint32_t chistory; //
uint8_t cprediction;
int ctrain;



//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//
uint32_t cal_mask(int bits)
{
  uint32_t mask = (1 << bits) - 1;
  return mask;
}

void init_gshare()
{
  ghistory = 0;

  size_t bht_size = 1 << ghistoryBits;
  gshareBHT = (uint8_t *)malloc(bht_size * sizeof(uint8_t));
  memset(gshareBHT, WN, bht_size * sizeof(uint8_t));
  gh_mask = cal_mask(ghistoryBits);

  // printf("gshare:        %10d, %d\n", ghistoryBits, gh_mask);
}

void init_tournament()
{
  ghistory = 0;

  size_t lPHT_size = 1 << pcIndexBits;
  lPHT = (uint32_t *)malloc(lPHT_size * sizeof(uint32_t));
  memset(lPHT, 0, lPHT_size * sizeof(uint32_t));

  size_t lBHT_size = 1 << lhistoryBits;
  lBHT = (uint8_t *)malloc(lBHT_size * sizeof(uint8_t));
  memset(lBHT, WN, lBHT_size * sizeof(uint8_t));

  size_t gBHT_size = 1 << ghistoryBits;
  gBHT = (uint8_t *)malloc(gBHT_size * sizeof(uint8_t));
  memset(gBHT, WN, gBHT_size * sizeof(uint8_t));

  selector = (uint8_t *)malloc(gBHT_size * sizeof(uint8_t));
  memset(selector, 0, gBHT_size * sizeof(uint8_t));

  lprediction = NOTTAKEN;
  gprediction = NOTTAKEN;

  pc_mask = cal_mask(pcIndexBits);
  lh_mask = cal_mask(lhistoryBits);
  gh_mask = cal_mask(ghistoryBits);


}

void init_custom()
{
  customBits = 22;
  ghistoryBits = 13;
  customHeights = (1 << customBits);
  theta = 1.93 * customBits + 14;

  chistory = 0;
  ghistory = 0;

  weights = (int **)malloc(customHeights * sizeof(int*));
  for (int i = 0; i < customHeights; ++i)
  {
    weights[i] = (int *)malloc((customBits + 1) * sizeof(int));
    memset(weights[i], 0, sizeof(int) * customBits);
    weights[i][0] = 1;
  }

  size_t bht_size = 1 << ghistoryBits;
  gshareBHT = (uint8_t *)malloc(bht_size * sizeof(uint8_t));
  memset(gshareBHT, WN, bht_size * sizeof(uint8_t));

  selector = (uint8_t *)malloc(bht_size * sizeof(uint8_t));
  memset(selector, 0, bht_size * sizeof(uint8_t));

  cprediction = NOTTAKEN;
  gprediction = NOTTAKEN;

  gh_mask = cal_mask(ghistoryBits);
  pc_mask = cal_mask(customBits);
}

void init_predictor()
{
  switch (bpType)
  {
  case STATIC:
    break;

  case GSHARE:
    init_gshare();
    break;

  case TOURNAMENT:
    init_tournament();
    break;

  case CUSTOM:
    init_custom();
    break;

  default:
    break;
  }
}



// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t make_prediction_gshare(uint32_t pc)
{
  int index = (pc ^ ghistory) & gh_mask;
  uint8_t prediction = NOTTAKEN;
  if (gshareBHT[index] == WT || gshareBHT[index] == ST)
  {
    prediction = TAKEN;
  }
  return prediction;
}

uint8_t make_prediction_tournament(uint32_t pc)
{
  uint32_t lPHT_index = pc & pc_mask;
  uint32_t lhistory = lPHT[lPHT_index];
  lprediction = NOTTAKEN;
  if (lBHT[lhistory] == WT || lBHT[lhistory] == ST)
  {
    lprediction = TAKEN;
  }

  gprediction = NOTTAKEN;
  if (gBHT[ghistory] == WT || gBHT[ghistory] == ST)
  {
    gprediction = TAKEN;
  }

  uint8_t select = selector[ghistory];
  if (select == 0 || select == 1)
  {
    return gprediction;
  }
  else if (select == 2 || select == 3)
  {
    return lprediction;
  }
  else
  {
    exit(1);
  }
}

uint8_t make_prediction_custom(uint32_t pc)
{
  uint32_t index = pc & pc_mask;
  int16_t out = weights[index][0];

  for (int i = 0; i < customBits; ++i)
  {
    out += ((chistory >> i) & 1) ? weights[index][i + 1] : -weights[index][i + 1];
  }

  cprediction = (out >= 0) ? TAKEN : NOTTAKEN;

  ctrain = out < theta && out > -theta;

  gprediction = make_prediction_gshare(pc);

  uint8_t select = selector[ghistory];
  if (select == 0 || select == 1)
  {
    return gprediction;
  }
  else if (select == 2 || select == 3)
  {
    return cprediction;
  }
  else
  {
    exit(1);
  }
}

uint8_t make_prediction(uint32_t pc)
{
  switch (bpType)
  {
  case STATIC:
    return TAKEN;

  case GSHARE:
    return make_prediction_gshare(pc);
    break;

  case TOURNAMENT:
    return make_prediction_tournament(pc);
    break;

  case CUSTOM:
    return make_prediction_custom(pc);
    break;

  default:
    break;
  }

  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//
void train_gshare(uint32_t pc, uint8_t outcome)
{
  int index = (pc ^ ghistory) & gh_mask;

  // printf("train_gshare %d, %d \n", index, outcome);
  // printf("train_gshare before:        %d, %d\n", gshareBHT[index], ghistory);

  if (outcome == TAKEN)
  {
    if (gshareBHT[index] != ST)
    {
      gshareBHT[index] = gshareBHT[index] + 1;
    }
  }
  else
  {
    if (gshareBHT[index] != SN)
    {
      gshareBHT[index] = gshareBHT[index] - 1;
    }
  }

  ghistory = ((ghistory << 1) | outcome) & gh_mask;


  // printf("train_gshare after:        %d, %d\n", gshareBHT[index], ghistory);
}

void train_tournament(uint32_t pc, uint8_t outcome)
{
  uint8_t select = selector[ghistory];
  if (outcome == lprediction && outcome != gprediction && select < 3)
  {
    selector[ghistory] = select + 1;
  }
  else if (outcome != lprediction && outcome == gprediction && select > 0)
  {
    selector[ghistory] = select - 1;
  }

  uint32_t lPHT_index = pc & pc_mask;
  uint32_t lhistory = lPHT[lPHT_index];
  if (outcome == TAKEN)
  {
    if (lBHT[lhistory] != ST)
    {
      lBHT[lhistory] = lBHT[lhistory] + 1;
    }
  }
  else
  {
    if (lBHT[lhistory] != SN)
    {
      lBHT[lhistory] = lBHT[lhistory] - 1;
    }
  }

  if (outcome == TAKEN)
  {
    if (gBHT[ghistory] != ST)
    {
      gBHT[ghistory] = gBHT[ghistory] + 1;
    }
  }
  else
  {
    if (gBHT[ghistory] != SN)
    {
      gBHT[ghistory] = gBHT[ghistory] - 1;
    }
  }

  lhistory = ((lhistory << 1) | outcome) & lh_mask;
  lPHT[lPHT_index] = lhistory;

  ghistory = ((ghistory << 1) | outcome) & gh_mask;
}

void train_custom(uint32_t pc, uint8_t outcome)
{
  uint8_t select = selector[ghistory];
  if (outcome == cprediction && outcome != gprediction && select < 3)
  {
    selector[ghistory] = select + 1;
  }
  else if (outcome != cprediction && outcome == gprediction && select > 0)
  {
    selector[ghistory] = select - 1;
  }

  uint32_t index = pc & pc_mask;

  if((cprediction != outcome) || ctrain){
    if(outcome == TAKEN)
      weights[index][0]++;
    else
      weights[index][0]--;

    for (int i = 0; i < customBits; ++i)
    {
      if(((chistory >> i) & 1) == outcome){
        weights[index][i + 1]++;
      }else{
        weights[index][i + 1]--;
      }
    }
  }

  chistory = ((chistory << 1) | outcome) & pc_mask;

  train_gshare(pc, outcome);
  // printf("gshare:        %d, %d\n", outcome, chistory);


}

void train_predictor(uint32_t pc, uint8_t outcome)
{
  switch (bpType)
  {
  case STATIC:
    break;

  case GSHARE:
    train_gshare(pc, outcome);
    break;

  case TOURNAMENT:
    train_tournament(pc, outcome);
    break;

  case CUSTOM:
    train_custom(pc, outcome);
    break;

  default:
    break;
  }
}
