/*******************************************************************************
################################################################################
#   Copyright (c) [2020] [HCL Technologies Ltd.]                               #
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

/* This file contains methods of Session/Connection creation and Subscription to
   YANG modules */

#include <stdio.h>
#include <stdlib.h>
#include "sysrepo.h"
#include "SessionHandler.hpp"
#include "InitConfig.hpp"
#include "NrCellCb.hpp"
#include "NrCellDuCb.hpp"
#include "RrmPolicyCb.hpp"

using namespace std;
/* Default constructor */
SessionHandler::SessionHandler()
{    
}


/* Destructor */
SessionHandler::~SessionHandler()
{
}

/********************************************************************** 
   Description : This function will create Connection, Session, and 
                 subscribe. These sysrepo class provide netconf connection
                 related services.
   Params[In]  : void
   Return      : true  - started successful
                 false - start failed
**********************************************************************/
bool SessionHandler::init()
{
   try
   {
      mConn = createConnection();
      if(mConn != NULL)
      {
         O1_LOG("\nO1 SessionHandler : Connection created");
         //removing nacm module temperary for auth issue resolve
         //mConn.remove_module("ietf-netconf-acm");
         mSess = createSession(mConn);
         if(mSess != NULL)
         {
            O1_LOG("\nO1 SessionHandler : Session created");
            mSub  = createSubscribe(mSess);
            if(mSub != NULL)
            {
               O1_LOG("\nO1 SessionHandler : Subscription created");
               if(InitConfig::instance().init(mSess))
               {
                  return true;
               }
               else
               {
                  return false;
               }
            }
            else 
            {
               O1_LOG("\nO1 SessionHandler : Subscription failed");
               return false;
            }
         }
         else
         {
            O1_LOG("\nO1 SessionHandler : Session failed");
            return false;
         }
      }
      else
      {
         O1_LOG("\nO1 SessionHandler : connection failed");
         return false;
      }
   }
   catch( const std::exception& e )
   {
      O1_LOG("\nO1 SessionHandler : Exception : %s", e.what());
      return false;
   }
}

/********************************************************************** 
   Description : This function will create Connection instance and 
                 return the same  
   Params[In]  : void
   Return      : sysrepo::S_Connection instance
**********************************************************************/
sysrepo::S_Connection SessionHandler::createConnection()
{
   sysrepo::S_Connection conn(new sysrepo::Connection());
   return conn;
}


/********************************************************************** 
   Description : This function will create Session instance and
                 return the same
   Params[In]  : sysrepo::S_Connection
   Return      : sysrepo::S_Session instance
**********************************************************************/
sysrepo::S_Session SessionHandler::createSession(sysrepo::S_Connection conn)
{
   sysrepo::S_Session sess(new sysrepo::Session(conn));
   return sess;
}



/**********************************************************************
   Description : This function will create Subscribe instance and
                 return the same
   Params[In]  : sysrepo::S_Session
   Return      : sysrepo::S_Subscribe instance
**********************************************************************/
sysrepo::S_Subscribe SessionHandler::createSubscribe(sysrepo::S_Session sess)
{
   sysrepo::S_Subscribe subscrb(new sysrepo::Subscribe(sess));
   if(subscribeModule(subscrb))
   {
      O1_LOG("\nO1 SessionHandler : Subscription done successfully");
   }
   return subscrb;
}


/********************************************************************** 
   Description : This function will create a callback object and register
                 it for callback. 
   Params[In]  : sysrepo::S_Subscribe
   Return      : true   - on success
**********************************************************************/
namespace SessionHandlerHelper
{
   #define OPER_GET_ITEMS_FUNC(NAMESPACE) \
      int oper_get_items(sysrepo::S_Session session, \
                      const char *module_name, \
                      const char *path, \
                      const char *request_xpath, \
                      uint32_t request_id, \
                      libyang::S_Data_Node &parent){ \
      ::NAMESPACE model; \
      return model.oper_get_items(session, module_name, path, request_xpath, request_id, std::ref(parent)); \
   };
   #define MODULE_CHANGE_FUNC(NAMESPACE) \
      int module_change(sysrepo::S_Session sess, \
                      const char *module_name, \
                      const char *xpath, \
                      sr_event_t event, \
                      uint32_t request_id){ \
      ::NAMESPACE model; \
      return model.module_change(sess, module_name, xpath, event, request_id); \
   };
   namespace AlarmOranYangModel{
      OPER_GET_ITEMS_FUNC(AlarmOranYangModel);
   };
   namespace NrCellCb{
      OPER_GET_ITEMS_FUNC(NrCellCb);
      MODULE_CHANGE_FUNC(NrCellCb);
   };
   namespace NrCellDuCb{
      OPER_GET_ITEMS_FUNC(NrCellDuCb);
      MODULE_CHANGE_FUNC(NrCellDuCb);
   };
   namespace RrmPolicyCb{
      OPER_GET_ITEMS_FUNC(RrmPolicyCb);
      MODULE_CHANGE_FUNC(RrmPolicyCb);
   };
};

bool SessionHandler::subscribeModule(sysrepo::S_Subscribe subscrb)
{
   // AlarmOranYangModel
   subscrb->oper_get_items_subscribe(ALARM_MODULE_NAME_ORAN, \
                                     *SessionHandlerHelper::AlarmOranYangModel::oper_get_items, \
                                     ALARM_MODULE_PATH_ORAN);

   // NrCellCb
   subscrb->oper_get_items_subscribe(CELL_STATE_MODULE_NAME, \
                                     *SessionHandlerHelper::NrCellCb::oper_get_items, \
                                     CELL_STATE_MODULE_PATH);

   subscrb->module_change_subscribe(CELL_STATE_MODULE_NAME, *SessionHandlerHelper::NrCellCb::module_change);

   // NrCellDuCb
   subscrb->oper_get_items_subscribe(MANAGED_ELEMENT_MODULE_NAME, \
                                     *SessionHandlerHelper::NrCellDuCb::oper_get_items, \
                                     MANAGED_ELEMENT_MODULE_PATH);

   subscrb->module_change_subscribe(MANAGED_ELEMENT_MODULE_NAME, *SessionHandlerHelper::NrCellDuCb::module_change);

   // RrmPolicyCb
   subscrb->oper_get_items_subscribe(RRMPOLICY_MODULE_NAME, \
                                     *SessionHandlerHelper::RrmPolicyCb::oper_get_items, \
                                     RRMPOLICY_MODULE_PATH);

   subscrb->module_change_subscribe(RRMPOLICY_MODULE_NAME, *SessionHandlerHelper::RrmPolicyCb::module_change);

   return true;
}

/**********************************************************************
         End of file
**********************************************************************/
