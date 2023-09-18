#ifndef SYSREPO_EXTEND_HPP
#define SYSREPO_EXTEND_HPP
#include "sysrepo-cpp/Sysrepo.hpp"
#include "sysrepo-cpp/Session.hpp"

namespace sysrepo{
  class Callback{
    public:
    Callback();
    virtual ~Callback();

    /** Wrapper for [sr_module_change_cb](@ref sr_module_change_cb) callback.*/
    virtual int module_change(S_Session session, const char *module_name, const char *xpath, sr_event_t event, \
          uint32_t request_id) {return SR_ERR_OK;};
    /** Wrapper for [sr_rpc_cb](@ref sr_rpc_cb) callback.*/
    virtual int rpc(S_Session session, const char *op_path, const S_Vals input, sr_event_t event, uint32_t request_id, \
          S_Vals_Holder output) {return SR_ERR_OK;};
    /** Wrapper for [sr_rpc_tree_cb](@ref sr_rpc_tree_cb) callback.*/
    virtual int rpc_tree(S_Session session, const char *op_path, const libyang::S_Data_Node input, sr_event_t event, \
          uint32_t request_id, libyang::S_Data_Node output) {return SR_ERR_OK;};
    /** Wrapper for [sr_event_notif_cb](@ref sr_event_notif_cb) callback.*/
    virtual void event_notif(S_Session session, const sr_ev_notif_type_t notif_type, const char *path, const S_Vals vals, \
          time_t timestamp) {return;};
    /** Wrapper for [sr_event_notif_tree_cb](@ref sr_event_notif_tree_cb) callback.*/
    virtual void event_notif_tree(S_Session session, const sr_ev_notif_type_t notif_type, const libyang::S_Data_Node notif, \
          time_t timestamp) {return;};
    /** Wrapper for [sr_oper_get_items_cb](@ref sr_oper_get_items_cb) callback.*/
    virtual int oper_get_items(S_Session session, const char *module_name, const char *path, const char *request_xpath, \
          uint32_t request_id, libyang::S_Data_Node &parent) {return SR_ERR_OK;};
    Callback *get() {return this;};

  };
  using S_Callback = std::shared_ptr<Callback>;
};
#endif