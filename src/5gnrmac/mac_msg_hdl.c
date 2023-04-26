/*******************************************************************************
################################################################################
#   Copyright (c) [2017-2019] [Radisys]                                        #
#                                                                              #
#   Licensed under the Apache License, Version 2.0 (the "License");            #
#   you may not use this file except in compliance with the License.           #
#   You may obtain a copy of the License at                                    #
#                                                                              #
#       http://www.apache.org/licenses/LICENSE-2.0                             #
#                                                                              #
#   Unless required by applicable law or agreed to in writing, software        #
#   distributed under the License is distributed on an "AS IS" BASIS,          #
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   #
#   See the License for the specific language governing permissions and        #
#   limitations under the License.                                             #
################################################################################
 *******************************************************************************/

/* header include files -- defines (.h)  */
#include "common_def.h"
#include "lrg.h"
#include "lrg.x"
#include "du_app_mac_inf.h"
#include "mac_sch_interface.h"
#include "rlc_mac_inf.h"
#include "mac_upr_inf_api.h"
#include "lwr_mac.h"
#ifdef INTEL_FAPI
#include "fapi_interface.h"
#endif
#include "lwr_mac_fsm.h"
#include "lwr_mac_upr_inf.h"
#include "mac.h"
#include "mac_utils.h"
#include "mac_harq_dl.h"

/* This file contains message handling functionality for MAC */

MacCb  macCb;

/*******************************************************************
 *
 * @brief Sends DL BO Info to SCH
 *
 * @details
 *
 *    Function : sendDlRlcBoInfoToSch
 *
 *    Functionality:
 *       Sends DL BO Info to SCH
 *
 * @params[in] 
 * @return ROK     - success
 *         RFAILED - failure
 *
 ****************************************************************/
uint8_t sendDlRlcBoInfoToSch(DlRlcBoInfo *dlBoInfo)
{
   Pst pst;

   FILL_PST_MAC_TO_SCH(pst, EVENT_DL_RLC_BO_INFO_TO_SCH);
   return(SchMessageRouter(&pst, (void *)dlBoInfo));
}

/*******************************************************************
 *
 * @brief Sends CRC Indication to SCH
 *
 * @details
 *
 *    Function : sendCrcIndMacToSch 
 *
 *    Functionality:
 *       Sends CRC Indication to SCH
 *
 * @params[in] 
 * @return ROK     - success
 *         RFAILED - failure
 *
 ****************************************************************/
uint8_t sendCrcIndMacToSch(CrcIndInfo *crcInd)
{
   Pst pst;

   FILL_PST_MAC_TO_SCH(pst, EVENT_CRC_IND_TO_SCH);
   return(SchMessageRouter(&pst, (void *)crcInd));
}

/*******************************************************************
 *
 * @brief Sends UL CQI Indication to SCH
 *
 * @details
 *
 *    Function : sendUlCqiIndMacToSch 
 *
 *    Functionality:
 *       Sends Ul Cqi Indication to SCH
 *
 * @params[in] 
 * @return ROK     - success
 *         RFAILED - failure
 *
 ****************************************************************/
uint8_t sendUlCqiIndMacToSch(SchUlCqiInd *ulCqiInd)
{
   Pst pst;

   FILL_PST_MAC_TO_SCH(pst, EVENT_UL_CQI_TO_SCH);
   return(SchMessageRouter(&pst, (void *)ulCqiInd));
}

/*******************************************************************
 *
 * @brief Sends DL CQI Indication to SCH
 *
 * @details
 *
 *    Function : sendDlCqiIndMacToSch 
 *
 *    Functionality:
 *       Sends Dl Cqi Indication to SCH
 *
 * @params[in] 
 * @return ROK     - success
 *         RFAILED - failure
 *
 ****************************************************************/
uint8_t sendDlCqiIndMacToSch(SchDlCqiInd *dlCqiInd)
{
   Pst pst;
   
   memset(&pst, 0, sizeof(Pst));
   FILL_PST_MAC_TO_SCH(pst, EVENT_DL_CQI_TO_SCH);
   return(SchMessageRouter(&pst, (void *)dlCqiInd));
}

uint16_t computeCRIBitLength(uint16_t cellId, uint16_t ueId)
{
   uint16_t bitlen = 0;
   uint8_t nb_resources = 1;
   // the template of OAI source code: nb_resources = csi_MeasConfig->nzp_CSI_RS_ResourceSetToAddModList->list.array[csi_idx]->nzp_CSI_RS_Resources.list.count;
   bitlen = ceil(log2(nb_resources));
   DU_LOG("\nDEBUG  -->  MAC: the bit length of CRI is %d", bitlen);

   return bitlen;
}

uint8_t computeNumberOfRIBitSet(uint8_t restriction_bit)
{
   uint8_t nb_of_bits_set = 0;
   uint8_t mask = 0xff;

   for(int index=7; (restriction_bit & mask) && (index>=0)  ; index--)
   {
      if (restriction_bit & (1<<index))
      {
         nb_of_bits_set++;
      }
      mask >>= 1;
   }

   return nb_of_bits_set;
}

uint16_t computeRIBitLength(uint16_t cellId, uint16_t ueId)
{
   uint16_t bitlen = 0;
   MacUeCb *ueCb = &macCb.macCell[cellId]->ueCb[ueId-1];
   CodebookConfig *codebookCfg = &ueCb->cellCb->ueRecfgTmpData[ueId-1]->spCellRecfg.servCellCfg.csiMeasCfg.csiRprtCfgToAddModList->codebookConfig;
   CsiReportContent *reportCntnt = &ueCb->cellCb->ueRecfgTmpData[ueId-1]->spCellRecfg.servCellCfg.csiMeasCfg.csiRprtCfgToAddModList->reportConfig;
   uint8_t restriction_bit, nb_allowed_ri = 0;
   
   // base on 38.212 table 6.3.1.1.2-3
   // 1 antenna ports = 0, 2 antenna ports = 1, 4 antenna ports = 2, more than 4 antenna ports = 3
   if(codebookCfg == NULL)
   {
      DU_LOG("\nDEBUG  -->  MAC: currently running on single antenna port.");
      reportCntnt->ri_bitlen = 0;
      return 0;
   }

   if(codebookCfg->codebookType.isType1)
   {
      if(codebookCfg->codebookType.type1.subType.isSinglePanel)
      {
         if(codebookCfg->codebookType.type1.subType.singlePanel.nrOfAntennaPorts.isTwoPort)
         {
            DU_LOG("\nDEBUG  -->  MAC: currently running on two antenna ports.");
            restriction_bit = codebookCfg->codebookType.type1.subType.singlePanel.ri_restriction_bit;
            nb_allowed_ri = computeNumberOfRIBitSet(restriction_bit);
            bitlen = ceil(log2(nb_allowed_ri));

            bitlen = bitlen<1?bitlen:1;
            reportCntnt->ri_bitlen = bitlen;
         }
         else if(codebookCfg->codebookType.type1.subType.singlePanel.nrOfAntennaPorts.isMoreThanTwoPort)
         {
            if(codebookCfg->codebookType.type1.subType.singlePanel.nrOfAntennaPorts.moreThanTwoPort.antennaConfig == TWO_ONE)
            {
               DU_LOG("\nDEBUG  -->  MAC: currently running on four antenna ports.");
               restriction_bit = codebookCfg->codebookType.type1.subType.singlePanel.ri_restriction_bit;
               nb_allowed_ri = computeNumberOfRIBitSet(restriction_bit);
               bitlen = ceil(log2(nb_allowed_ri));

               bitlen = bitlen<1?bitlen:2;
               reportCntnt->ri_bitlen = bitlen;
            }
            else
            {
               DU_LOG("\nDEBUG  -->  MAC: currently running on more than four antenna ports.");
               restriction_bit = codebookCfg->codebookType.type1.subType.singlePanel.ri_restriction_bit;
               nb_allowed_ri = computeNumberOfRIBitSet(restriction_bit);
               bitlen = ceil(log2(nb_allowed_ri));

               reportCntnt->ri_bitlen = bitlen;
            }
         }
         else
         {
            DU_LOG("\nERROR  -->  MAC: antenna config. is wrong.");
         }
      }
      else
      {
         DU_LOG("\nERROR  -->  MAC: only support codebook type I single panel.");
      }
   }
   else
   {
      DU_LOG("\nERROR  -->  MAC: only support codebook type I.");
   }

   return bitlen;
}

uint16_t computePMIBitLength(uint16_t cellId, uint16_t ueId)
{
   uint16_t bitlen = 0;
   MacUeCb *ueCb = &macCb.macCell[cellId]->ueCb[ueId-1];
   CodebookConfig *codebookCfg = &ueCb->cellCb->ueRecfgTmpData[ueId-1]->spCellRecfg.servCellCfg.csiMeasCfg.csiRprtCfgToAddModList->codebookConfig;
   CsiReportContent *reportCntnt = &ueCb->cellCb->ueRecfgTmpData[ueId-1]->spCellRecfg.servCellCfg.csiMeasCfg.csiRprtCfgToAddModList->reportConfig;
   uint8_t restriction_bit = codebookCfg->codebookType.type1.subType.singlePanel.ri_restriction_bit;

   // check each number of layers one by one
   for(int i=0; i<8; i++) 
   {
      reportCntnt->pmi_x1_bitlen[i] = 0;
      reportCntnt->pmi_x2_bitlen[i] = 0;
      if(codebookCfg == NULL || restriction_bit == 0)
      {
         return bitlen;
      }
      else
      {
         if(codebookCfg->codebookType.isType1)
         {
            if(codebookCfg->codebookType.type1.subType.isSinglePanel)
            {
               if(codebookCfg->codebookType.type1.subType.singlePanel.nrOfAntennaPorts.isTwoPort)
               {
                  // two ports
                  if(i==0)
                  {
                     reportCntnt->pmi_x2_bitlen[i] = 2;
                  }
                  if(i==1)
                  {
                     reportCntnt->pmi_x2_bitlen[i] = 1;
                  }
               }
               else if(codebookCfg->codebookType.type1.subType.singlePanel.nrOfAntennaPorts.isMoreThanTwoPort)
               {
                  // more than two ports
                  int n1, n2, o1, o2, x1, x2;
                  // Table 5.2.2.2.1-2 in 38.214 for supported configurations, and we only consider four antenna ports so far.
                  switch (codebookCfg->codebookType.type1.subType.singlePanel.nrOfAntennaPorts.moreThanTwoPort.antennaConfig)
                  {
                     case (TWO_ONE):
                     {
                        // assume it is codebook mode 1
                        n1 = 2;
                        n2 = 1;
                        o1 = 4;
                        o2 = 1;
                     }
                     break;
                     case (TWO_TWO):
                     {
                        n1 = 2;
                        n2 = 2;
                        o1 = 4;
                        o2 = 4;
                     }
                     break;
                     case (FOUR_ONE):
                     {
                        n1 = 4;
                        n2 = 1;
                        o1 = 4;
                        o2 = 1;
                     }
                     break;
                     default:
                        DU_LOG("\nERROR  -->  MAC: it doesn't support other antenna config. yet (we only consider 4 antenna ports).");
                     break;
                  }

                  int rank = i+1;
                  int codebook_mode = codebookCfg->codebookType.type1.codebook_mode;
                  switch(rank) // the number of rank
                  {
                     case 1:
                     {
                        if(n2 > 1)
                        {
                           if(codebook_mode == 1)
                           {
                              x1 = ceil(log2(n1*o1)) + ceil(log2(n2*o2));
                              x2 = 2;
                           }
                           else
                           {
                              x1 = ceil(log2(n1*o1/2)) + ceil(log2(n2*o2/2));
                              x2 = 4;
                           }
                        }
                        else
                        {
                           if(codebook_mode == 1)
                           {
                              x1 = ceil(log2(n1*o1)) + ceil(log2(n2*o2));
                              x2 = 2;
                           }
                           else
                           {
                              x1 = ceil(log2(n1*o1/2));
                              x2 = 4;
                           }
                        }
                     }
                     break;
                     case 2:
                     {
                        if(n1*n2 == 2) 
                        {
                           if (codebook_mode == 1) 
                           {
                              x1 = ceil(log2(n1*o1)) + ceil(log2(n2*o2));
                              x2 = 1;
                           }
                           else 
                           {
                              x1 = ceil(log2(n1*o1/2));
                              x2 = 3;
                           }
                           x1 += 1;
                        }
                        else {
                           if(n2>1) 
                           {
                              if (codebook_mode == 1) 
                              {
                                 x1 = ceil(log2(n1*o1)) + ceil(log2(n2*o2));
                                 x2 = 3;
                              }
                              else 
                              {
                                 x1 = ceil(log2(n1*o1/2)) + ceil(log2(n2*o2/2));
                                 x2 = 3;
                              }
                           }
                           else
                           {
                              if (codebook_mode == 1) 
                              {
                                 x1 = ceil(log2(n1*o1)) + ceil(log2(n2*o2));
                                 x2 = 1;
                              }
                              else 
                              {
                                 x1 = ceil(log2(n1*o1/2));
                                 x2 = 3;
                              }
                           }
                           x1 += 2;
                        }
                     }
                     break;
                     case 3:
                     case 4:
                     {
                        if(n1*n2 == 2) 
                        {
                           x1 = ceil(log2(n1*o1)) + ceil(log2(n2*o2));
                           x2 = 1;
                        }
                        else 
                        {
                           if(n1*n2 >= 8) 
                           {
                              x1 = ceil(log2(n1*o1/2)) + ceil(log2(n2*o2)) + 2;
                              x2 = 1;
                           }
                           else 
                           {
                              x1 = ceil(log2(n1*o1)) + ceil(log2(n2*o2)) + 2;
                              x2 = 1;
                           }
                        }
                     }
                     break;
                     case 5:
                     case 6:
                     {
                        x1 = ceil(log2(n1*o1)) + ceil(log2(n2*o2));
                        x2 = 1;
                     }
                     break;
                     case 7:
                     case 8:
                     {
                        if(n1 == 4 && n2 == 1) 
                        {
                           x1 = ceil(log2(n1*o1/2)) + ceil(log2(n2*o2));
                           x2 = 1;
                        }
                        else 
                        {
                           if(n1 > 2 && n2 == 2) 
                           {
                              x1 = ceil(log2(n1*o1)) + ceil(log2(n2*o2/2));
                              x2 = 1;
                           }
                           else 
                           {
                              x1 = ceil(log2(n1*o1)) + ceil(log2(n2*o2));
                              x2 = 1;
                           }
                        }
                     }
                     break;
                     default:
                     {
                        DU_LOG("\nERROR  -->  MAC: invalid rank for PMI bit length computation.");
                     }
                     break;
                  }

                  reportCntnt->pmi_x1_bitlen[i] = x1;
                  reportCntnt->pmi_x2_bitlen[i] = x2;
                  bitlen = x2;

               }
               else
               {
                  DU_LOG("\nERROR  -->  MAC: antenna config. is wrong.");
               }
            }
            else
            {
               DU_LOG("\nERROR  -->  MAC: only support codebook type I single panel.");
            }
         }
         else
         {
            DU_LOG("\nERROR  -->  MAC: only support codebook type I.");
         }
      }
   }
   
   return bitlen;
}

uint16_t computeCQIBitLength(uint16_t cellId, uint16_t ueId)
{
   uint16_t bitlen = 0;
   MacUeCb *ueCb = &macCb.macCell[cellId]->ueCb[ueId-1];
   CodebookConfig *codebookCfg = &ueCb->cellCb->ueRecfgTmpData[ueId-1]->spCellRecfg.servCellCfg.csiMeasCfg.csiRprtCfgToAddModList->codebookConfig;
   CsiReportContent *reportCntnt = &ueCb->cellCb->ueRecfgTmpData[ueId-1]->spCellRecfg.servCellCfg.csiMeasCfg.csiRprtCfgToAddModList->reportConfig;

   if(codebookCfg == NULL)
   {
      for(int i=0; i<8; i++)
      {
         reportCntnt->cqi_bitlen[i] = 0;
      }
      return 0;
   }
   // check each number of layers one by one
   for (int i=0; i<8; i++)
   {
      uint8_t restriction_bit = codebookCfg->codebookType.type1.subType.singlePanel.ri_restriction_bit;
      if((restriction_bit>>i)&0x01)
      {
         reportCntnt->cqi_bitlen[i] = 4;
         if(codebookCfg != NULL)
         {
            if(codebookCfg->codebookType.isType1)
            {
               if(codebookCfg->codebookType.type1.subType.isSinglePanel)
               {
                  if(codebookCfg->codebookType.type1.subType.singlePanel.nrOfAntennaPorts.isMoreThanTwoPort)
                  {
                     bitlen = 4;
                     if(codebookCfg->codebookType.type1.subType.singlePanel.nrOfAntennaPorts.moreThanTwoPort.antennaConfig > TWO_ONE)
                     {
                        // more than four antenna ports
                        if(i >= 4)
                        {
                           reportCntnt->cqi_bitlen[i] += 4; // CQI for second TB
                           bitlen = 8;
                        }
                     }
                  }
               }
            }
         }
      }
      else
      {
         reportCntnt->cqi_bitlen[i] = 0;
      }
   }

   return bitlen;
}

uint8_t pickandreverse_bits(uint8_t *payload, uint16_t bitlen, int start_bit) 
{
  uint8_t rev_bits = 0;
  for (int i=0; i<bitlen; i++)
  {
   rev_bits |= ((payload[(start_bit+i)/8]>>((start_bit+i)%8))&0x01)<<(bitlen-1-i);
  }
    
  return rev_bits;
}

uint16_t evaluateCRIReport(uint8_t *payload, uint8_t cri_bitlen, int cumul_bits, uint16_t cellId, uint16_t ueId)
{
   uint16_t CRI = pickandreverse_bits(payload, cri_bitlen, cumul_bits);

   return CRI;
}

uint16_t evaluateRIReport(uint8_t *payload, uint8_t ri_bitlen, int cumul_bits, uint16_t cellId, uint16_t ueId)
{
   uint16_t RI = pickandreverse_bits(payload, ri_bitlen, cumul_bits);
   MacUeCb *ueCb = &macCb.macCell[cellId]->ueCb[ueId-1];
   CodebookConfig *codebookCfg = &ueCb->cellCb->ueRecfgTmpData[ueId-1]->spCellRecfg.servCellCfg.csiMeasCfg.csiRprtCfgToAddModList->codebookConfig;
   uint8_t restriction_bit = codebookCfg->codebookType.type1.subType.singlePanel.ri_restriction_bit;

   for(int i=0; i<8; i++)
   {
      if ((restriction_bit>>i)&0x01 && i==RI) 
      { 
         return RI;
      }
   }

   DU_LOG("\nERROR  -->  MAC: RI is not qualified for RI restriction.");
   return 0;
}

uint16_t evaluatePMIReport(uint8_t *payload, uint8_t pmi_bitlen, int cumul_bits, uint16_t cellId, uint16_t ueId)
{
    uint16_t PMI = 3;

    return PMI;
}

uint16_t evaluateCQIReport(uint8_t *payload, uint8_t cqi_bitlen, int cumul_bits, uint16_t cellId, uint16_t ueId)
{
    uint16_t CQI = 4;

    return CQI;
}

uint8_t extractCSIReport(SchDlCqiInd *dlCqiInd, UciInd *macUciInd, UciPucchF2F3F4 *uciPucchF2F3F4)
{
   uint8_t ret = ROK;
   uint16_t cellIdx = 0, ueId = 0;
   MacUeCb *ueCb = NULLP;
   uint16_t CRIBitLength = 0, RIBitLength = 0, PMIBitLength = 0, CQIBitLength = 0, cumul_bits = 0;
   uint8_t *payload = uciPucchF2F3F4->uciBits;

   GET_CELL_IDX(macUciInd->cellId, cellIdx);
   dlCqiInd->cellId = macCb.macCell[cellIdx]->cellId;
   
   dlCqiInd->crnti = uciPucchF2F3F4->crnti;
   GET_UE_ID(dlCqiInd->crnti, ueId);
   ueCb = &macCb.macCell[cellIdx]->ueCb[ueId-1];
   
   // CRIBitLength = computeCRIBitLength(cellIdx, ueId);
   // RIBitLength = computeRIBitLength(cellIdx, ueId);
   // PMIBitLength = computePMIBitLength(cellIdx, ueId);
   // CQIBitLength = computeCQIBitLength(cellIdx, ueId);

   // assume there is no zero padding
   dlCqiInd->dlCqiRpt.reportType = 0b1111;
   dlCqiInd->dlCqiRpt.cri = evaluateCRIReport(payload, CRIBitLength, cumul_bits, cellIdx, ueId);
   cumul_bits += CRIBitLength;

   dlCqiInd->dlCqiRpt.ri = evaluateRIReport(payload, RIBitLength, cumul_bits, cellIdx, ueId);
   cumul_bits += RIBitLength;
   
   dlCqiInd->dlCqiRpt.pmi = evaluatePMIReport(payload, PMIBitLength, cumul_bits, cellIdx, ueId);
   cumul_bits += PMIBitLength;
   
   dlCqiInd->dlCqiRpt.cqi = evaluateCQIReport(payload, CQIBitLength, cumul_bits, cellIdx, ueId);

   return ret;
}

/*******************************************************************
 *
 * @brief Sends Power Headroom Indication to SCH
 *
 * @details
 *
 *    Function : sendPhrIndToSch 
 *
 *    Functionality:
 *       Sends Phr Indication to SCH
 *
 * @params[in] 
 * @return ROK     - success
 *         RFAILED - failure
 *
 ****************************************************************/
uint8_t sendPhrIndToSch(SchPwrHeadroomInd *macPhrInd)
{
   Pst pst;

   FILL_PST_MAC_TO_SCH(pst, EVENT_PHR_IND_TO_SCH);
   return(SchMessageRouter(&pst, (void *)macPhrInd));
}

/*******************************************************************
 *
 * @brief Processes CRC Indication from PHY
 *
 * @details
 *
 *    Function : fapiMacCrcInd
 *
 *    Functionality:
 *       Processes CRC Indication from PHY
 *
 * @params[in] Post Structure Pointer
 *             Crc Indication Pointer
 * @return ROK     - success
 *         RFAILED - failure
 *
 * ****************************************************************/
uint8_t fapiMacCrcInd(Pst *pst, CrcInd *crcInd)
{
   uint16_t     cellIdx;
   CrcIndInfo   crcIndInfo;
   DU_LOG("\nDEBUG  -->  MAC : Received CRC indication");
   GET_CELL_IDX(crcInd->cellId, cellIdx);
   /* Considering one pdu and one preamble */ 
   crcIndInfo.cellId = macCb.macCell[cellIdx]->cellId;
   crcIndInfo.crnti = crcInd->crcInfo[0].rnti;
   crcIndInfo.timingInfo.sfn = crcInd->timingInfo.sfn;
   crcIndInfo.timingInfo.slot = crcInd->timingInfo.slot;
   crcIndInfo.numCrcInd = crcInd->crcInfo[0].numCb;
   crcIndInfo.crcInd[0] = crcInd->crcInfo[0].cbCrcStatus[0];

   MAC_FREE_SHRABL_BUF(pst->region, pst->pool, crcInd, sizeof(CrcInd));
   return(sendCrcIndMacToSch(&crcIndInfo));
}

/*******************************************************************
 *
 * @brief Process Rx Data Ind at MAC
 *
 * @details
 *
 *    Function : fapiMacRxDataInd
 *
 *    Functionality:
 *       Process Rx Data Ind at MAC
 *
 * @params[in] Post structure
 *             Rx Data Indication
 * @return ROK     - success
 *         RFAILED - failure
 *
 * ****************************************************************/
uint8_t fapiMacRxDataInd(Pst *pst, RxDataInd *rxDataInd)
{
   uint8_t ueId = 0;
   uint16_t pduIdx, cellIdx = 0;
   DU_LOG("\nDEBUG  -->  MAC : Received Rx Data indication");
   /* TODO : compare the handle received in RxDataInd with handle send in PUSCH
    * PDU, which is stored in raCb */

   for(pduIdx = 0; pduIdx < rxDataInd->numPdus; pduIdx++)
   {

      GET_CELL_IDX(rxDataInd->cellId, cellIdx);
      GET_UE_ID(rxDataInd->pdus[pduIdx].rnti, ueId);
      
      if(macCb.macCell[cellIdx] && macCb.macCell[cellIdx]->ueCb[ueId -1].transmissionAction == STOP_TRANSMISSION)
      {
         DU_LOG("\nINFO   -->  MAC : UL data transmission not allowed for UE %d", macCb.macCell[cellIdx]->ueCb[ueId -1].ueId);
      }
      else
      {
         unpackRxData(rxDataInd->cellId, rxDataInd->timingInfo, &rxDataInd->pdus[pduIdx]);
      }
      MAC_FREE_SHRABL_BUF(pst->region, pst->pool, rxDataInd->pdus[pduIdx].pduData,\
         rxDataInd->pdus[pduIdx].pduLength);
   }
   MAC_FREE_SHRABL_BUF(pst->region, pst->pool, rxDataInd, sizeof(RxDataInd));
   return ROK;
}

/*******************************************************************
 *
 * @brief Processes DL data from RLC
 *
 * @details
 *
 *    Function : MacProcRlcDlData 
 *
 *    Functionality:
 *      Processes DL data from RLC
 *
 * @params[in] Post structure
 *             DL data
 * @return ROK     - success
 *         RFAILED - failure
 *
 * ****************************************************************/
uint8_t MacProcRlcDlData(Pst* pstInfo, RlcDlData *dlData)
{
   uint8_t   pduIdx = 0;
   uint8_t   ueId  = 0;
   uint8_t   lcIdx = 0;
   uint8_t   *txPdu = NULLP;
   uint16_t  cellIdx = 0, txPduLen = 0;
   MacDlData macDlData;
   MacDlSlot *currDlSlot = NULLP;
   DlRlcBoInfo dlBoInfo;

   memset(&macDlData , 0, sizeof(MacDlData));
   DU_LOG("\nDEBUG  -->  MAC: Received DL data for sfn=%d slot=%d numPdu= %d", \
      dlData->slotInfo.sfn, dlData->slotInfo.slot, dlData->numPdu);

   GET_UE_ID(dlData->rnti, ueId);   

   /* Copy the pdus to be muxed into mac Dl data */
   macDlData.ueId = ueId;
   macDlData.numPdu = dlData->numPdu;
   for(pduIdx = 0;  pduIdx < dlData->numPdu; pduIdx++)
   {
      macDlData.pduInfo[pduIdx].lcId = dlData->pduInfo[pduIdx].lcId;
      macDlData.pduInfo[pduIdx].pduLen = dlData->pduInfo[pduIdx].pduLen;
      macDlData.pduInfo[pduIdx].dlPdu = dlData->pduInfo[pduIdx].pduBuf;
   }

   GET_CELL_IDX(dlData->cellId, cellIdx);
   /* Store DL data in the scheduled slot */
   if(macCb.macCell[cellIdx] ==NULLP)
   {
      DU_LOG("\nERROR  -->  MAC : MacProcRlcDlData(): macCell does not exists");
      return RFAILED;
   }
   currDlSlot = &macCb.macCell[cellIdx]->dlSlot[dlData->slotInfo.slot];
   if(currDlSlot->dlInfo.dlMsgAlloc[ueId-1])
   {
      if(currDlSlot->dlInfo.dlMsgAlloc[ueId-1]->dlMsgPdschCfg)
      {
         txPduLen = currDlSlot->dlInfo.dlMsgAlloc[ueId-1]->dlMsgPdschCfg->codeword[0].tbSize\
                    - TX_PAYLOAD_HDR_LEN;
         MAC_ALLOC(txPdu, txPduLen);
         if(!txPdu)
         {
            DU_LOG("\nERROR  -->  MAC : Memory allocation failed in MacProcRlcDlData");
            return RFAILED;
         }
         macMuxPdu(&macDlData, NULLP, txPdu, txPduLen);

         currDlSlot->dlInfo.dlMsgAlloc[ueId-1]->dlMsgPduLen = txPduLen;
         currDlSlot->dlInfo.dlMsgAlloc[ueId-1]->dlMsgPdu = txPdu;
         /* Add muxed TB to DL HARQ Proc CB. This will be used if retranmission of
          * TB is requested in future. */
         updateNewTbInDlHqProcCb(dlData->slotInfo, &macCb.macCell[cellIdx]->ueCb[ueId -1], \
               currDlSlot->dlInfo.dlMsgAlloc[ueId-1]->dlMsgPdschCfg->codeword[0].tbSize, txPdu);
      }
   }

   for(lcIdx = 0; lcIdx < dlData->numLc; lcIdx++)
   {
      if(dlData->boStatus[lcIdx].bo)
      {
         memset(&dlBoInfo, 0, sizeof(DlRlcBoInfo));
         dlBoInfo.cellId = dlData->boStatus[lcIdx].cellId;
         GET_CRNTI(dlBoInfo.crnti, dlData->boStatus[lcIdx].ueId);
         dlBoInfo.lcId = dlData->boStatus[lcIdx].lcId;
         dlBoInfo.dataVolume = dlData->boStatus[lcIdx].bo;
         sendDlRlcBoInfoToSch(&dlBoInfo);
      }
   }

   /* Free memory */
   for(pduIdx = 0; pduIdx < dlData->numPdu; pduIdx++)
   {
      MAC_FREE_SHRABL_BUF(pstInfo->region, pstInfo->pool, dlData->pduInfo[pduIdx].pduBuf,\
            dlData->pduInfo[pduIdx].pduLen);
   }
   if(pstInfo->selector == ODU_SELECTOR_LWLC)
   {
      MAC_FREE_SHRABL_BUF(pstInfo->region, pstInfo->pool, dlData, sizeof(RlcDlData));
   }
   return ROK;
}

/*******************************************************************
 *
 * @brief Builds and Sends UL Data to RLC
 *
 * @details
 *
 *    Function : macProcUlData
 *
 *    Functionality: Builds and Sends UL Data to RLC
 *
 * @params[in] CellId
 *             CRNTI
 *             Slot information
 *             LC Id on which payload was received
 *             Pointer to the payload
 *             Length of payload
 * @return ROK     - success
 *         RFAILED - failure
 *
 * ****************************************************************/
uint8_t macProcUlData(uint16_t cellId, uint16_t rnti, SlotTimingInfo slotInfo, \
uint8_t lcId, uint16_t pduLen, uint8_t *pdu)
{
   Pst         pst;
   RlcUlData     *ulData;

   /* Filling RLC Ul Data*/
   MAC_ALLOC_SHRABL_BUF(ulData, sizeof(RlcUlData));
   if(!ulData)
   {
      DU_LOG("\nERROR  -->  MAC : Memory allocation failed while sending UL data to RLC");
      return RFAILED;
   }
   memset(ulData, 0, sizeof(RlcUlData));
   ulData->cellId = cellId; 
   ulData->rnti = rnti;
   memcpy(&ulData->slotInfo, &slotInfo, sizeof(SlotTimingInfo));
   ulData->slotInfo.cellId = cellId;

   /* Filling pdu info */
   ulData->pduInfo[ulData->numPdu].lcId = lcId;
   ulData->pduInfo[ulData->numPdu].pduBuf = pdu;
   ulData->pduInfo[ulData->numPdu].pduLen = pduLen;
   ulData->numPdu++;

   /* Filling Post and send to RLC */
   memset(&pst, 0, sizeof(Pst));
   FILL_PST_MAC_TO_RLC(pst, 0, EVENT_UL_DATA_TO_RLC);
   MacSendUlDataToRlc(&pst, ulData);

   return ROK;
}


/*******************************************************************
 *
 * @brief Processes BO status from RLC
 *
 * @details
 *
 *    Function : MacProcRlcBOStatus
 *
 *    Functionality:
 *      Processes BO status from RLC
 *
 * @params[in] Post structure
 *             BO status
 * @return ROK     - success
 *         RFAILED - failure
 *
 * ****************************************************************/
uint8_t MacProcRlcBoStatus(Pst* pst, RlcBoStatus* boStatus)
{
   DlRlcBoInfo  dlBoInfo;

   dlBoInfo.cellId = boStatus->cellId;
   GET_CRNTI(dlBoInfo.crnti, boStatus->ueId);
   dlBoInfo.lcId = boStatus->lcId;
   dlBoInfo.dataVolume = boStatus->bo;
   
   sendDlRlcBoInfoToSch(&dlBoInfo); 

   if(pst->selector == ODU_SELECTOR_LWLC)
   {
      MAC_FREE_SHRABL_BUF(pst->region, pst->pool, boStatus, sizeof(RlcBoStatus));
   }

   return ROK;
}

/*******************************************************************
 *
 * @brief Send LC schedule result report to RLC
 *
 * @details
 *
 *    Function : sendSchRptToRlc 
 *
 *    Functionality: Send LC schedule result report to RLC
 *
 * @params[in] 
 * @return ROK     - success
 *         RFAILED - failure
 *
 * ****************************************************************/
uint8_t sendSchedRptToRlc(DlSchedInfo dlInfo, SlotTimingInfo slotInfo, uint8_t ueIdx, uint8_t schInfoIdx)
{
   Pst      pst;
   uint8_t  lcIdx;
   RlcSchedResultRpt  *schedRpt = NULLP;
   
   MAC_ALLOC_SHRABL_BUF(schedRpt, sizeof(RlcSchedResultRpt));
   if(!schedRpt)
   {
      DU_LOG("\nERROR  -->  MAC: Memory allocation failure in sendSchResultRepToRlc");
      return RFAILED;
   }

   DU_LOG("\nDEBUG  -->  MAC: Send scheduled result report for sfn %d slot %d", slotInfo.sfn, slotInfo.slot);
   schedRpt->cellId = dlInfo.cellId;
   schedRpt->slotInfo.sfn = slotInfo.sfn;
   schedRpt->slotInfo.slot = slotInfo.slot;

   if(dlInfo.dlMsgAlloc[ueIdx])
   {
      schedRpt->rnti = dlInfo.dlMsgAlloc[ueIdx]->crnti;
      schedRpt->numLc = dlInfo.dlMsgAlloc[ueIdx]->transportBlock[0].numLc;
      for(lcIdx = 0; lcIdx < schedRpt->numLc; lcIdx++)
      {
         schedRpt->lcSch[lcIdx].lcId = dlInfo.dlMsgAlloc[ueIdx]->transportBlock[0].lcSchInfo[lcIdx].lcId;
         schedRpt->lcSch[lcIdx].bufSize = dlInfo.dlMsgAlloc[ueIdx]->transportBlock[0].lcSchInfo[lcIdx].schBytes;
      }
   }

   /* Fill Pst */
   FILL_PST_MAC_TO_RLC(pst, RLC_DL_INST, EVENT_SCHED_RESULT_TO_RLC);
   if(MacSendSchedResultRptToRlc(&pst, schedRpt) != ROK)
   {
      DU_LOG("\nERROR  -->  MAC: Failed to send Schedule result report to RLC");
      MAC_FREE_SHRABL_BUF(MAC_MEM_REGION, MAC_POOL, schedRpt, sizeof(RlcSchedResultRpt));
      return RFAILED;
   }

   return ROK;
}

/*******************************************************************
 *
 * @brief Handles cell start reuqest from DU APP
 *
 * @details
 *
 *    Function : MacProcCellStart
 *
 *    Functionality:
 *      Handles cell start reuqest from DU APP
 *
 * @params[in] Post structure pointer
 *             Cell Id 
 * @return ROK     - success
 *         RFAILED - failure
 *
 * ****************************************************************/
uint8_t MacProcCellStart(Pst *pst, CellStartInfo  *cellStart)
{
   DU_LOG("\nINFO  -->  MAC : Handling cell start request");
   gSlotCount = 0;
   sendToLowerMac(START_REQUEST, 0, cellStart);

   MAC_FREE_SHRABL_BUF(pst->region, pst->pool, cellStart, \
	 sizeof(CellStartInfo));

   return ROK;
}

/*******************************************************************
 *
 * @brief Handles cell stop from DU APP
 *
 * @details
 *
 *    Function : MacProcCellStop
 *
 *    Functionality:
 *        Handles cell stop from DU APP
 *
 * @params[in] Post structure pointer
 *             Cell Id
 * @return ROK     - success
 *         RFAILED - failure
 *
 * ****************************************************************/
uint8_t MacProcCellStop(Pst *pst, CellStopInfo  *cellStop)
{
#ifdef INTEL_FAPI
   uint16_t      cellIdx; 

   DU_LOG("\nINFO  -->  MAC : Sending cell stop request to Lower Mac");
   GET_CELL_IDX(cellStop->cellId, cellIdx);
   if(macCb.macCell[cellIdx])
   {
      macCb.macCell[cellIdx]->state = CELL_TO_BE_STOPPED;
   }
#endif

   MAC_FREE_SHRABL_BUF(pst->region, pst->pool, cellStop, \
	 sizeof(CellStopInfo));

   return ROK;
}

/*******************************************************************
 *
 * @brief Handles DL CCCH Ind from DU APP
 *
 * @details
 *
 *    Function : MacProcDlCcchInd 
 *
 *    Functionality:
 *      Handles DL CCCH Ind from DU APP
 *
 * @params[in] Post structure pointer
 *             DL CCCH Ind pointer 
 * @return ROK     - success
 *         RFAILED - failure
 *
 * ****************************************************************/
uint8_t MacProcDlCcchInd(Pst *pst, DlCcchIndInfo *dlCcchIndInfo)
{
   uint8_t      ueId = 0, ueIdx = 0;
   uint16_t     cellIdx;
   uint16_t     idx;
   DlRlcBoInfo  dlBoInfo;
   memset(&dlBoInfo, 0, sizeof(DlRlcBoInfo));

   DU_LOG("\nDEBUG  -->  MAC : Handling DL CCCH IND");

   GET_CELL_IDX(dlCcchIndInfo->cellId, cellIdx);

   dlBoInfo.cellId = dlCcchIndInfo->cellId;
   dlBoInfo.crnti = dlCcchIndInfo->crnti;

   if(dlCcchIndInfo->msgType == RRC_SETUP)
   {
      dlBoInfo.lcId = SRB0_LCID;    // SRB ID 0 for msg4
      /* (MSG4 pdu + 3 bytes sub-header) + (Contention resolution id MAC CE + 1 byte sub-header) */
      dlBoInfo.dataVolume = (dlCcchIndInfo->dlCcchMsgLen + 3) + (MAX_CRI_SIZE + 1);

      /* storing Msg4 Pdu in raCb */
      GET_UE_ID(dlBoInfo.crnti, ueId);
      ueIdx = ueId -1;
      if(macCb.macCell[cellIdx]->macRaCb[ueIdx].crnti == dlCcchIndInfo->crnti)
      {
	 macCb.macCell[cellIdx]->macRaCb[ueIdx].msg4PduLen = dlCcchIndInfo->dlCcchMsgLen;
	 MAC_ALLOC(macCb.macCell[cellIdx]->macRaCb[ueIdx].msg4Pdu, \
	    macCb.macCell[cellIdx]->macRaCb[ueIdx].msg4PduLen);
	 if(macCb.macCell[cellIdx]->macRaCb[ueIdx].msg4Pdu)
	 {
	    for(idx = 0; idx < dlCcchIndInfo->dlCcchMsgLen; idx++)
	    {
	       macCb.macCell[cellIdx]->macRaCb[ueIdx].msg4Pdu[idx] =\
	          dlCcchIndInfo->dlCcchMsg[idx];
	    }
	 }
      }
   }
   sendDlRlcBoInfoToSch(&dlBoInfo);

   MAC_FREE_SHRABL_BUF(pst->region, pst->pool, dlCcchIndInfo->dlCcchMsg, \
	 dlCcchIndInfo->dlCcchMsgLen);
   MAC_FREE_SHRABL_BUF(pst->region, pst->pool, dlCcchIndInfo, sizeof(DlCcchIndInfo));
   return ROK;

}

/*******************************************************************
 *
 * @brief Sends UL CCCH Ind to DU APP
 *
 * @details
 *
 *    Function : macProcUlCcchInd
 *
 *    Functionality:
 *        MAC sends UL CCCH Ind to DU APP
 *
 * @params[in] Post structure pointer
 *            
 * @return ROK     - success
 *         RFAILED - failure
 *
 * ****************************************************************/
uint8_t macProcUlCcchInd(uint16_t cellId, uint16_t crnti, uint16_t rrcContSize, uint8_t *rrcContainer)
{
   Pst pst;
   uint8_t ret = ROK;
   UlCcchIndInfo *ulCcchIndInfo = NULLP;

   MAC_ALLOC_SHRABL_BUF(ulCcchIndInfo, sizeof(UlCcchIndInfo));
   if(!ulCcchIndInfo)
   {
      DU_LOG("\nERROR  -->  MAC: Memory failed in macProcUlCcchInd");
      return RFAILED;
   }

   ulCcchIndInfo->cellId = cellId;
   ulCcchIndInfo->crnti  = crnti;
   ulCcchIndInfo->ulCcchMsgLen = rrcContSize;
   ulCcchIndInfo->ulCcchMsg = rrcContainer;

   /* Fill Pst */
   FILL_PST_MAC_TO_DUAPP(pst, EVENT_MAC_UL_CCCH_IND);

   if(MacDuAppUlCcchInd(&pst, ulCcchIndInfo) != ROK)
   {
      DU_LOG("\nERROR  -->  MAC: Failed to send UL CCCH Ind to DU APP");
      MAC_FREE_SHRABL_BUF(MAC_MEM_REGION, MAC_POOL, ulCcchIndInfo->ulCcchMsg, ulCcchIndInfo->ulCcchMsgLen);
      MAC_FREE_SHRABL_BUF(MAC_MEM_REGION, MAC_POOL, ulCcchIndInfo, sizeof(UlCcchIndInfo));
      ret = RFAILED;
   }
   return ret;
}

/*******************************************************************
 *
 * @brief Processes received short BSR
 *
 * @details
 *
 *    Function : macProcShortBsr
 *
 *    Functionality:
 *        MAC sends Short BSR to SCH
 *
 * @params[in] cell ID
 *             crnti
 *             lcg ID
 *             buffer size
 *
 * @return ROK     - success
 *         RFAILED - failure
 *
 * ****************************************************************/
uint8_t macProcShortBsr(uint16_t cellId, uint16_t crnti, uint8_t lcgId, uint32_t bufferSize)
{
   Pst                  pst;
   UlBufferStatusRptInd bsrInd;

   memset(&pst, 0, sizeof(Pst));
   memset(&bsrInd, 0, sizeof(UlBufferStatusRptInd));

   bsrInd.cellId                 = cellId;
   bsrInd.crnti                  = crnti;
   bsrInd.bsrType                = SHORT_BSR;
   bsrInd.numLcg                 = 1; /* short bsr reports one lcg at a time */
   bsrInd.dataVolInfo[0].lcgId   = lcgId;
   bsrInd.dataVolInfo[0].dataVol = bufferSize;

   FILL_PST_MAC_TO_SCH(pst, EVENT_SHORT_BSR);
   return(SchMessageRouter(&pst, (void *)&bsrInd));
}

/*******************************************************************
 *
 * @brief Processes received short BSR
 *
 * @details
 *
 *    Function : macProcShortBsr
 *
 *    Functionality:
 *        MAC sends Short BSR to SCH
 *
 * @params[in] cell ID
 *             crnti
 *             lcg ID
 *             buffer size
 *
 * @return ROK     - success
 *         RFAILED - failure
 *
 * ****************************************************************/
uint8_t macProcLongBsr(uint16_t cellId, uint16_t crnti,uint8_t numLcg,\
                         DataVolInfo dataVolInfo[MAX_NUM_LOGICAL_CHANNEL_GROUPS])
{
   Pst                  pst;
   UlBufferStatusRptInd bsrInd;
   uint8_t lcgIdx = 0;

   memset(&pst, 0, sizeof(Pst));
   memset(&bsrInd, 0, sizeof(UlBufferStatusRptInd));

   bsrInd.cellId                 = cellId;
   bsrInd.crnti                  = crnti;
   bsrInd.bsrType                = LONG_BSR;
   bsrInd.numLcg                 = numLcg; 

   for(lcgIdx = 0; lcgIdx < numLcg; lcgIdx++)
      memcpy(&(bsrInd.dataVolInfo[lcgIdx]), &(dataVolInfo[lcgIdx]), sizeof(DataVolInfo));

   FILL_PST_MAC_TO_SCH(pst, EVENT_LONG_BSR);
   return(SchMessageRouter(&pst, (void *)&bsrInd));
}

/*******************************************************************
 *
 * @brief Builds and send DL HARQ Indication to SCH
 *
 * @details
 *
 *    Function : buildAndSendHarqInd
 *
 *    Functionality:
 *       Builds and send HARQ UCI Indication to SCH
 *
 * @params[in] harqInfo Pointer
 *             crnti value
 *             cell Index value
 *             slot Ind Pointer
 * @return ROK     - success
 *         RFAILED - failure
 *
 * ****************************************************************/
uint8_t buildAndSendHarqInd(HarqInfoF0F1 *harqInfo, uint8_t crnti, uint16_t cellIdx, SlotTimingInfo *slotInd)
{
   uint16_t harqCounter=0;
   Pst pst;
   DlHarqInd   dlHarqInd;
   memset(&pst, 0, sizeof(Pst));
   memset(&dlHarqInd, 0, sizeof(DlHarqInd));

   dlHarqInd.cellId       = macCb.macCell[cellIdx]->cellId;
   dlHarqInd.crnti        = crnti;
   dlHarqInd.slotInd.sfn  = slotInd->sfn;
   dlHarqInd.slotInd.slot = slotInd->slot;
   dlHarqInd.numHarq      = harqInfo->numHarq;
   memset(dlHarqInd.harqPayload, 0, MAX_SR_BITS_IN_BYTES);
   for(harqCounter = 0; harqCounter < harqInfo->numHarq; harqCounter++)
   {
      dlHarqInd.harqPayload[harqCounter] = harqInfo->harqValue[harqCounter];
   }
   
   /* Fill Pst */
   FILL_PST_MAC_TO_SCH(pst, EVENT_DL_HARQ_IND_TO_SCH);

   return SchMessageRouter(&pst, (void *)&dlHarqInd);
}


/*******************************************************************
 *
 * @brief Builds and send SR UCI Indication to SCH
 *
 * @details
 *
 *    Function : buildAndSendSrInd
 *
 *    Functionality:
 *       Builds and send SR UCI Indication to SCH
 *
 * @params[in] SrUciIndInfo Pointer
 *             crnti value
 * @return ROK     - success
 *         RFAILED - failure
 *
 * ****************************************************************/
uint8_t buildAndSendSrInd(UciInd *macUciInd, uint8_t crnti)
{
   uint16_t cellIdx;
   Pst pst;
   SrUciIndInfo   srUciInd;
   memset(&pst, 0, sizeof(Pst));
   memset(&srUciInd, 0, sizeof(SrUciIndInfo));

   GET_CELL_IDX(macUciInd->cellId, cellIdx);
   srUciInd.cellId       = macCb.macCell[cellIdx]->cellId;
   srUciInd.crnti        = crnti;
   srUciInd.slotInd.sfn  = macUciInd->slotInd.sfn;
   srUciInd.slotInd.slot = macUciInd->slotInd.slot;
   srUciInd.numSrBits++;
   memset(srUciInd.srPayload, 0, MAX_SR_BITS_IN_BYTES);

   /* Fill Pst */
   FILL_PST_MAC_TO_SCH(pst, EVENT_UCI_IND_TO_SCH);

   return(SchMessageRouter(&pst, (void *)&srUciInd));
}

/*******************************************************************
 *
 * @brief Processes UCI Indication from PHY
 *
 * @details
 *
 *    Function : fapiMacUciInd
 *
 *    Functionality:
 *       Processes UCI Indication from PHY
 *
 * @params[in] Post Structure Pointer
 *             UCI Indication Pointer
 * @return ROK     - success
 *         RFAILED - failure
 *
 * ****************************************************************/
uint8_t FapiMacUciInd(Pst *pst, UciInd *macUciInd)
{
   uint8_t     pduIdx = 0, ret = ROK;
   uint16_t    nPdus = 0, crnti = 0, cellIdx = 0;

   if(macUciInd)
   {
      nPdus = macUciInd->numUcis;
      while(nPdus)
      {
         switch(macUciInd->pdus[pduIdx].pduType)
         {
            case UCI_IND_PUSCH:
               break;
            case UCI_IND_PUCCH_F0F1:
            {
               {
                  DU_LOG("\nDEBUG  -->  MAC : Received HARQ UCI Indication\n");
                  GET_CELL_IDX(macUciInd->cellId, cellIdx);
                  buildAndSendHarqInd(&macUciInd->pdus[pduIdx].uci.uciPucchF0F1.harqInfo, macUciInd->pdus[pduIdx].uci.uciPucchF0F1.crnti, cellIdx, &macUciInd->slotInd);
               }
               if(macUciInd->pdus[pduIdx].uci.uciPucchF0F1.srInfo.srIndPres)
               {
                  DU_LOG("\nDEBUG  -->  MAC : Received SR UCI indication");
                  crnti = macUciInd->pdus[pduIdx].uci.uciPucchF0F1.crnti; 
                  ret = buildAndSendSrInd(macUciInd, crnti);
               }
            }
               break;
            case UCI_IND_PUCCH_F2F3F4:
               {
                  if(macUciInd->pdus[pduIdx].uci.uciPucchF2F3F4.pduBitmap & SR_PDU_BITMASK)
                  {
                     // TODO: so far PUCCH format 2/3/4 does not support SR payload
                     DU_LOG("\nERROR  -->  MAC: SR not yet support for PUCCH format 2/3/4");
                  }
                  if(macUciInd->pdus[pduIdx].uci.uciPucchF2F3F4.pduBitmap & HARQ_PDU_BITMASK)
                  {
                     // TODO: so far PUCCH format 2/3/4 does not support HARQ payload
                     DU_LOG("\nERROR  -->  MAC: HARQ not yet support for PUCCH format 2/3/4");
                  }
                  if(macUciInd->pdus[pduIdx].uci.uciPucchF2F3F4.pduBitmap & CSI_PART1_PDU_BITMASK)
                  {
                     DU_LOG("\nDEBUG  -->  MAC : Received CSI Report UCI indication");
                     
                     SchDlCqiInd *CSIReport = NULLP;
                     MAC_ALLOC(CSIReport, sizeof(SchDlCqiInd));
                     memset(CSIReport, 0, sizeof(SchDlCqiInd));
                     memset(&CSIReport->dlCqiRpt, 0, sizeof(DlCqiReport));
                     
                     ret = extractCSIReport(CSIReport, macUciInd, &macUciInd->pdus[pduIdx].uci.uciPucchF2F3F4);
                     ret = sendDlCqiIndMacToSch(CSIReport);
                     MAC_FREE(CSIReport, sizeof(SchDlCqiInd));
                  }
                  if(macUciInd->pdus[pduIdx].uci.uciPucchF2F3F4.pduBitmap & CSI_PART2_PDU_BITMASK)
                  {
                     // TODO: handle CSI Report payload part2
                     DU_LOG("\nERROR  -->  MAC: CSI Report payload2 not yet support for PUCCH format 2/3/4");
                  }
               }
               break;
            default:
               DU_LOG("\nERROR  -->  MAC: Invalid Pdu Type %d at FapiMacUciInd", macUciInd->pdus[pduIdx].pduType);
               ret = RFAILED;
               break;
         }
         pduIdx++;
         nPdus--;
      }
   }
   else
   {
      DU_LOG("\nERROR  -->  MAC: Received Uci Ind is NULL at FapiMacUciInd()");
      ret = RFAILED;
   }
   MAC_FREE_SHRABL_BUF(pst->region, pst->pool, macUciInd, sizeof(UciInd));
   return ret;
}

/*******************************************************************
 *
 * @brief fill Slice Cfg Request info in shared structre
 * 
 * @details
 *
 *    Function : fillSliceCfgInfo 
 *
 *    Functionality:
 *       fill Slice Cfg Request info in shared structre
 *
 * @params[in] SchSliceCfgReq *schSliceCfgReq 
 *             MacSliceCfgReq *macSliceCfgReq;
 * @return ROK     - success
 *         RFAILED - failure
 *
 **********************************************************************/
uint8_t fillSliceCfgInfo(SchSliceCfgReq *schSliceCfgReq, MacSliceCfgReq *macSliceCfgReq)
{
   uint8_t rrmPolicyIdx= 0,cfgIdx = 0, memberListIdx = 0, totalSliceCfgRecvd = 0;
 
   if(macSliceCfgReq->listOfRrmPolicy)
   {
      for(cfgIdx = 0; cfgIdx<macSliceCfgReq->numOfRrmPolicy; cfgIdx++)
      {
          totalSliceCfgRecvd += macSliceCfgReq->listOfRrmPolicy[cfgIdx]->numOfRrmPolicyMem;  
      }

      schSliceCfgReq->numOfConfiguredSlice =  totalSliceCfgRecvd;
      MAC_ALLOC(schSliceCfgReq->listOfSlices, schSliceCfgReq->numOfConfiguredSlice *sizeof(SchRrmPolicyOfSlice*));
      if(schSliceCfgReq->listOfSlices == NULLP)
      {
         DU_LOG("\nERROR  -->  MAC : Memory allocation failed in fillSliceCfgInfo");
         return RFAILED;
      }
      cfgIdx = 0; 

      for(rrmPolicyIdx = 0; rrmPolicyIdx<macSliceCfgReq->numOfRrmPolicy; rrmPolicyIdx++)
      {
         for(memberListIdx = 0; memberListIdx<macSliceCfgReq->listOfRrmPolicy[rrmPolicyIdx]->numOfRrmPolicyMem; memberListIdx++)
         {
            if(macSliceCfgReq->listOfRrmPolicy[rrmPolicyIdx]->rRMPolicyMemberList[memberListIdx])
            {

               MAC_ALLOC(schSliceCfgReq->listOfSlices[cfgIdx], sizeof(SchRrmPolicyOfSlice));
               if(schSliceCfgReq->listOfSlices[cfgIdx] == NULLP)
               {
                  DU_LOG("\nERROR  -->  MAC : Memory allocation failed in fillSliceCfgInfo");
                  return RFAILED;
               }

               memcpy(&schSliceCfgReq->listOfSlices[cfgIdx]->snssai, &macSliceCfgReq->listOfRrmPolicy[rrmPolicyIdx]->rRMPolicyMemberList[memberListIdx]->snssai, sizeof(Snssai));

               schSliceCfgReq->listOfSlices[cfgIdx]->rrmPolicyRatioInfo.maxRatio = macSliceCfgReq->listOfRrmPolicy[rrmPolicyIdx]->policyRatio.maxRatio;
               schSliceCfgReq->listOfSlices[cfgIdx]->rrmPolicyRatioInfo.minRatio = macSliceCfgReq->listOfRrmPolicy[rrmPolicyIdx]->policyRatio.minRatio;
               schSliceCfgReq->listOfSlices[cfgIdx]->rrmPolicyRatioInfo.dedicatedRatio = macSliceCfgReq->listOfRrmPolicy[rrmPolicyIdx]->policyRatio.dedicatedRatio;
               cfgIdx++;
            }
         }
      }
   }
   return ROK;
}

/*******************************************************************
 *
 * @brief Processes Slice Cfg Request recived from DU
 *
 * @details
 *
 *    Function : MacProcSliceCfgReq 
 *
 *    Functionality:
 *       Processes Processes Slice Cfg Request recived from DU
 *
 * @params[in] Post Structure Pointer
 *             MacSliceCfgReq *macSliceCfgReq;
 * @return ROK     - success
 *         RFAILED - failure
 *
 **********************************************************************/
uint8_t MacProcSliceCfgReq(Pst *pst, MacSliceCfgReq *macSliceCfgReq)
{
   uint8_t ret = ROK;
   Pst schPst;
   SchSliceCfgReq *schSliceCfgReq;

   DU_LOG("\nINFO  -->  MAC : Received Slice Cfg request from DU APP");
   if(macSliceCfgReq)
   {
      MAC_ALLOC(schSliceCfgReq, sizeof(SchSliceCfgReq));
      if(schSliceCfgReq == NULLP)
      {
         DU_LOG("\nERROR -->  MAC : Memory allocation failed in MacProcSliceCfgReq");
         ret = RFAILED;
      }
      else
      {
         if(fillSliceCfgInfo(schSliceCfgReq, macSliceCfgReq) == ROK)
         {
            FILL_PST_MAC_TO_SCH(schPst, EVENT_SLICE_CFG_REQ_TO_SCH);
            ret = SchMessageRouter(&schPst, (void *)schSliceCfgReq);
         }
      }
      freeMacSliceCfgReq(macSliceCfgReq, pst); 
   }
   else
   {
      DU_LOG("\nINFO  -->  MAC : Received MacSliceCfgReq is NULL");
   }
   return ret;
}

/*******************************************************************
 *
 * @brief Processes Slice ReCfg Request recived from DU
 *
 * @details
 *
 *    Function : MacProcSliceRecfgReq 
 *
 *    Functionality:
 *       Processes Processes Slice ReCfg Request recived from DU
 *
 * @params[in] Post Structure Pointer
 *             MacSliceCfgReq *macSliceRecfgReq;
 * @return ROK     - success
 *         RFAILED - failure
 *
 **********************************************************************/
uint8_t MacProcSliceRecfgReq(Pst *pst, MacSliceRecfgReq *macSliceRecfgReq)
{
   uint8_t ret = ROK;
   Pst schPst;
   SchSliceRecfgReq *schSliceRecfgReq;

   DU_LOG("\nINFO  -->  MAC : Received Slice ReCfg request from DU APP");
   if(macSliceRecfgReq)
   {
      MAC_ALLOC(schSliceRecfgReq, sizeof(SchSliceRecfgReq));
      if(schSliceRecfgReq == NULLP)
      {
         DU_LOG("\nERROR -->  MAC : Memory allocation failed in MacProcSliceRecfgReq");
         ret = RFAILED;
      }
      else
      {
         if(fillSliceCfgInfo(schSliceRecfgReq, macSliceRecfgReq) == ROK)
         {
            FILL_PST_MAC_TO_SCH(schPst, EVENT_SLICE_RECFG_REQ_TO_SCH);
            ret = SchMessageRouter(&schPst, (void *)schSliceRecfgReq);
         }
      }
      freeMacSliceCfgReq(macSliceRecfgReq, pst);
   }
   else
   {
      DU_LOG("\nINFO  -->  MAC : Received MacSliceRecfgReq is NULL");
   }
   return ret;
}

/**********************************************************************
  End of file
 **********************************************************************/

