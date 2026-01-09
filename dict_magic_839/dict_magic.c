#include "dict_magic_codes.h"
#include <freeDiameter/extension.h>

/**
 * @brief 创建新的词典对象并检查结果。
 * @details 这是一个函数式宏，用于简化词典对象的创建逻辑并处理可能的错误。
 *
 * @param _type 对象类型 (如 DICT_AVP, DICT_VENDOR)。
 * @param _data 包含对象定义的数据结构指针。
 * @param _parent 父级词典对象指针，如果无则为 NULL。
 * @param _ref 用于存储新创建对象指针的输出参数。
 * @return int 成功返回 0，失败则通过 CHECK_FCT 宏抛出错误。
 *
 * @warning 此宏会直接在调用处返回错误码，因此必须在返回类型为 int 且遵循 fd
 * 错误约定的函数中使用。
 */
#define CHECK_dict_new(_type, _data, _parent, _ref)                            \
  CHECK_FCT(fd_dict_new(fd_g_config->cnf_dict, (_type), (_data), (_parent),    \
                        (_ref)));

/**
 * @brief 搜索词典对象并检查结果。
 * @details 搜索指定的词典对象，如果未找到则返回 ENOENT。
 */
#define CHECK_dict_search(_type, _criteria, _what, _result)                    \
  CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, (_type), (_criteria),        \
                           (_what), (_result), ENOENT));

/**
 * @brief 本地规则定义结构体。
 * @details 用于在分组 AVP 或命令中定义子 AVP 的匹配规则。
 */
struct local_rules_definition {
  char *avp_name; ///< 子 AVP 的名称。
  enum rule_position
      position; ///< AVP 在消息中的位置 (如 RULE_REQUIRED, RULE_OPTIONAL)。
  int min;      ///< 最小出现次数 (-1 表示无限制)。
  int max;      ///< 最大出现次数 (-1 表示无限制)。
};

#define RULE_ORDER(_position)                                                  \
  ((((_position) == RULE_FIXED_HEAD) || ((_position) == RULE_FIXED_TAIL)) ? 1  \
                                                                          : 0)

#define PARSE_loc_rules(_rulearray, _parent)                                   \
  {                                                                            \
    int __ar;                                                                  \
    for (__ar = 0; __ar < sizeof(_rulearray) / sizeof((_rulearray)[0]);        \
         __ar++) {                                                             \
      struct dict_rule_data __data = {NULL, (_rulearray)[__ar].position, 0,    \
                                      (_rulearray)[__ar].min,                  \
                                      (_rulearray)[__ar].max};                 \
      __data.rule_order = RULE_ORDER(__data.rule_position);                    \
      /* First attempt: base AVP by name; suppress ENOENT error logging */     \
      int __rc = fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME,  \
                                (_rulearray)[__ar].avp_name, &__data.rule_avp, \
                                ENOENT);                                       \
      if (__rc != 0 && __rc != ENOENT)                                         \
        return __rc;                                                           \
      if (!__data.rule_avp) {                                                  \
        /* Fallback: explicit vendor search for AEEC (13712), no error log */  \
        struct dict_avp_request __aeec_req;                                    \
        __aeec_req.avp_vendor = 13712;                                         \
        __aeec_req.avp_name = (_rulearray)[__ar].avp_name;                     \
        __rc = fd_dict_search(fd_g_config->cnf_dict, DICT_AVP,                 \
                              AVP_BY_NAME_AND_VENDOR, &__aeec_req,             \
                              &__data.rule_avp, ENOENT);                       \
        if (__rc != 0 && __rc != ENOENT)                                       \
          return __rc;                                                         \
      }                                                                        \
      if (!__data.rule_avp) {                                                  \
        TRACE_DEBUG(INFO, "AVP Not found: '%s'", (_rulearray)[__ar].avp_name); \
        return ENOENT;                                                         \
      }                                                                        \
      CHECK_FCT_DO(fd_dict_new(fd_g_config->cnf_dict, DICT_RULE, &__data,      \
                               _parent, NULL),                                 \
                   {                                                           \
                     TRACE_DEBUG(INFO, "Error on rule with AVP '%s'",          \
                                 (_rulearray)[__ar].avp_name);                 \
                     return EINVAL;                                            \
                   });                                                         \
    }                                                                          \
  }

/**
 * @briefMAGIC 词典扩展入口函数。
 * @details 负责加载 ARINC 839-2014 协议定义的词典对象，包括：
 *          1. 注册厂商 ID 13712 (AEEC)。
 *          2. 注册应用 ID 1094202169 (MAGIC-ARINC839)。
 *          3. 注册所有 AVP（由 add_avps 实现）。
 *          4. 定义分组 AVP 的子规则和命令结构。
 *
 * @param conffile 配置文件路径（当前未使用）。
 * @return int 成功返回 0，失败返回错误码。
 *
 * @note 此函数作为 freeDiameter 的扩展加载点，由 EXTENSION_ENTRY 调用。
 */
static int dict_magic_arinc839_entry(char *conffile) {
  struct dict_object *magic_app = NULL;
  struct dict_object *magic_vendor = NULL;

  // 1. 注册供应商
  {
    struct dict_vendor_data vendor_data = {13712, "AEEC (ARINC)"};
    CHECK_dict_new(DICT_VENDOR, &vendor_data, NULL, &magic_vendor);
  }

  // 2. 注册应用
  {
    struct dict_application_data app_data = {1094202169, "MAGIC-ARINC839"};
    CHECK_dict_new(DICT_APPLICATION, &app_data, magic_vendor, &magic_app);
  }

  // 3. 调用 CSV 生成的 AVP 定义
  CHECK_FCT(add_avps());

  // 4. 定义所有 19 个分组 AVP 的规则
  // (在这里添加上面的规则定义代码)
  /* Grouped AVP Rules for ARINC 839-2014 MAGIC Diameter */

  // Communication-Request-Parameters (20001)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "Communication-Request-Parameters";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"Profile-Name", RULE_REQUIRED, -1, 1},
        {"Requested-Bandwidth", RULE_OPTIONAL, -1, 1},
        {"Requested-Return-Bandwidth", RULE_OPTIONAL, -1, 1},
        {"Required-Bandwidth", RULE_OPTIONAL, -1, 1},
        {"Required-Return-Bandwidth", RULE_OPTIONAL, -1, 1},
        {"Priority-Type", RULE_OPTIONAL, -1, 1},
        {"Accounting-Enabled", RULE_OPTIONAL, -1, 1},
        {"Priority-Class", RULE_OPTIONAL, -1, 1},
        {"DLM-Name", RULE_OPTIONAL, -1, 1},
        {"QoS-Level", RULE_OPTIONAL, -1, 1},
        {"Flight-Phase", RULE_OPTIONAL, -1, 1},
        {"Altitude", RULE_OPTIONAL, -1, 1},
        {"Airport", RULE_OPTIONAL, -1, 1},
        {"TFTtoGround-List", RULE_OPTIONAL, -1, 1},
        {"TFTtoAircraft-List", RULE_OPTIONAL, -1, 1},
        {"NAPT-List", RULE_OPTIONAL, -1, 1},
        {"Keep-Request", RULE_OPTIONAL, -1, 1},
        {"Auto-Detect", RULE_OPTIONAL, -1, 1},
        {"Timeout", RULE_OPTIONAL, -1, 1}};
    PARSE_loc_rules(rules, avp);
  }

  // Communication-Answer-Parameters (20002)
  // 注意：在错误应答（如链路不可用）中，只需包含基本信息
  // 因此大部分字段设为 OPTIONAL
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "Communication-Answer-Parameters";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"Profile-Name", RULE_OPTIONAL, -1, 1},
        {"Granted-Bandwidth", RULE_OPTIONAL, -1, 1},
        {"Granted-Return-Bandwidth", RULE_OPTIONAL, -1, 1},
        {"Priority-Type", RULE_OPTIONAL, -1, 1},
        {"Priority-Class", RULE_OPTIONAL, -1, 1},
        {"TFTtoGround-List", RULE_OPTIONAL, -1, 1},
        {"TFTtoAircraft-List", RULE_OPTIONAL, -1, 1},
        {"QoS-Level", RULE_OPTIONAL, -1, 1},
        {"Accounting-Enabled", RULE_OPTIONAL, -1, 1},
        {"DLM-Availability-List", RULE_OPTIONAL, -1, 1},
        {"Keep-Request", RULE_OPTIONAL, -1, 1},
        {"Auto-Detect", RULE_OPTIONAL, -1, 1},
        {"Timeout", RULE_OPTIONAL, -1, 1},
        {"Flight-Phase", RULE_OPTIONAL, -1, 1},
        {"Altitude", RULE_OPTIONAL, -1, 1},
        {"Airport", RULE_OPTIONAL, -1, 1},
        {"NAPT-List", RULE_OPTIONAL, -1, 1},
        {"Gateway-IPAddress", RULE_OPTIONAL, -1, 1},
        {"DLM-Name", RULE_OPTIONAL, -1, 1}};
    PARSE_loc_rules(rules, avp);
  }

  // Communication-Report-Parameters (20003)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "Communication-Report-Parameters";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"Profile-Name", RULE_REQUIRED, -1, 1},
        {"Granted-Bandwidth", RULE_OPTIONAL, -1, 1},
        {"Granted-Return-Bandwidth", RULE_OPTIONAL, -1, 1},
        {"Priority-Type", RULE_OPTIONAL, -1, 1},
        {"Priority-Class", RULE_OPTIONAL, -1, 1},
        {"TFTtoGround-List", RULE_OPTIONAL, -1, 1},
        {"TFTtoAircraft-List", RULE_OPTIONAL, -1, 1},
        {"QoS-Level", RULE_OPTIONAL, -1, 1},
        {"DLM-Availability-List", RULE_OPTIONAL, -1, 1},
        {"NAPT-List", RULE_OPTIONAL, -1, 1},
        {"Gateway-IPAddress", RULE_OPTIONAL, -1, 1}};
    PARSE_loc_rules(rules, avp);
  }

  // TFTtoGround-List (20004)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "TFTtoGround-List";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"TFTtoGround-Rule", RULE_REQUIRED, 1, 255}};
    PARSE_loc_rules(rules, avp);
  }

  // TFTtoAircraft-List (20005)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "TFTtoAircraft-List";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"TFTtoAircraft-Rule", RULE_REQUIRED, 1, 255}};
    PARSE_loc_rules(rules, avp);
  }

  // NAPT-List (20006)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "NAPT-List";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"NAPT-Rule", RULE_REQUIRED, 1, 255}};
    PARSE_loc_rules(rules, avp);
  }

  // DLM-List (20007)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "DLM-List";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"DLM-Info", RULE_REQUIRED, 1, -1}};
    PARSE_loc_rules(rules, avp);
  }

  // DLM-Info (20008)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "DLM-Info";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"DLM-Name", RULE_REQUIRED, -1, 1},
        {"DLM-Available", RULE_REQUIRED, -1, 1},
        {"DLM-Max-Links", RULE_REQUIRED, -1, 1},
        {"DLM-Max-Bandwidth", RULE_REQUIRED, -1, 1},
        {"DLM-Max-Return-Bandwidth", RULE_OPTIONAL, -1, 1},
        {"DLM-Allocated-Links", RULE_REQUIRED, -1, 1},
        {"DLM-Allocated-Bandwidth", RULE_REQUIRED, -1, 1},
        {"DLM-Allocated-Return-Bandwidth", RULE_OPTIONAL, -1, 1},
        {"DLM-QoS-Level-List", RULE_REQUIRED, -1, 1},
        {"DLM-Link-Status-List", RULE_OPTIONAL, -1, 1}};
    PARSE_loc_rules(rules, avp);
  }

  // DLM-QoS-Level-List (20009)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "DLM-QoS-Level-List";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"QoS-Level", RULE_REQUIRED, 0, 3}};
    PARSE_loc_rules(rules, avp);
  }

  // DLM-Link-Status-List (20010)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "DLM-Link-Status-List";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"Link-Status-Group", RULE_OPTIONAL, 0, -1}};
    PARSE_loc_rules(rules, avp);
  }

  // Link-Status-Group (20011)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "Link-Status-Group";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"Link-Name", RULE_REQUIRED, -1, 1},
        {"Link-Number", RULE_REQUIRED, -1, 1},
        {"Link-Available", RULE_REQUIRED, -1, 1},
        {"QoS-Level", RULE_REQUIRED, -1, 1},
        {"Link-Connection-Status", RULE_REQUIRED, -1, 1},
        {"Link-Login-Status", RULE_REQUIRED, -1, 1},
        {"Link-Max-Bandwidth", RULE_REQUIRED, -1, 1},
        {"Link-Max-Return-Bandwidth", RULE_OPTIONAL, -1, 1},
        {"Link-Alloc-Bandwidth", RULE_OPTIONAL, -1, 1},
        {"Link-Alloc-Return-Bandwidth", RULE_OPTIONAL, -1, 1},
        {"Link-Error-String", RULE_OPTIONAL, -1, 1}};
    PARSE_loc_rules(rules, avp);
  }

  // CDRs-Active (20012)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "CDRs-Active";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"CDR-Info", RULE_REQUIRED, 1, -1}};
    PARSE_loc_rules(rules, avp);
  }

  // CDRs-Finished (20013)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "CDRs-Finished";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"CDR-Info", RULE_REQUIRED, 1, -1}};
    PARSE_loc_rules(rules, avp);
  }

  // CDRs-Forwarded (20014)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "CDRs-Forwarded";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"CDR-Info", RULE_REQUIRED, 1, -1}};
    PARSE_loc_rules(rules, avp);
  }

  // CDRs-Unknown (20015)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "CDRs-Unknown";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {{"CDR-ID", RULE_REQUIRED, 1, -1}};
    PARSE_loc_rules(rules, avp);
  }

  // CDRs-Updated (20016)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "CDRs-Updated";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"CDR-Start-Stop-Pair", RULE_REQUIRED, 1, -1}};
    PARSE_loc_rules(rules, avp);
  }

  // CDR-Info (20017)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "CDR-Info";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"CDR-ID", RULE_REQUIRED, -1, 1},
        {"CDR-Content", RULE_OPTIONAL, -1, 1}};
    PARSE_loc_rules(rules, avp);
  }

  // CDR-Start-Stop-Pair (20018)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "CDR-Start-Stop-Pair";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"CDR-Stopped", RULE_REQUIRED, -1, 1},
        {"CDR-Started", RULE_REQUIRED, -1, 1}};
    PARSE_loc_rules(rules, avp);
  }

  // Client-Credentials (20019)
  {
    struct dict_object *avp;
    struct dict_avp_request vpa;
    vpa.avp_vendor = 13712;
    vpa.avp_name = "Client-Credentials";
    CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &avp);
    struct local_rules_definition rules[] = {
        {"User-Name", RULE_REQUIRED, -1, 1},
        {"Client-Password", RULE_REQUIRED, -1, 1}};
    PARSE_loc_rules(rules, avp);
  }

  // 5. 定义 7 对命令(code 350-356)
  // (添加命令定义代码)
  /* ARINC 839-2014 MAGIC Diameter Commands */

  // MCAR/MCAA - MAGIC-Client-Authentication (100000)
  {
    // Request
    struct dict_object *cmd;
    struct dict_cmd_data data = {
        100000,                                /* Code */
        "MAGIC-Client-Authentication-Request", /* Name */
        CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE |
            CMD_FLAG_ERROR,                   /* Fixed flags */
        CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE /* Fixed flag values */
    };
    CHECK_dict_new(DICT_COMMAND, &data, magic_app, &cmd);

    struct local_rules_definition rules[] = {
        {"Session-Id", RULE_FIXED_HEAD, -1, 1},
        {"Origin-Host", RULE_REQUIRED, -1, 1},
        {"Origin-Realm", RULE_REQUIRED, -1, 1},
        {"Destination-Realm", RULE_REQUIRED, -1, 1},
        {"Auth-Application-Id", RULE_REQUIRED, -1, 1},
        {"Session-Timeout", RULE_OPTIONAL, -1, 1},
        {"Client-Credentials", RULE_OPTIONAL, -1, 1},
        {"Auth-Session-State", RULE_OPTIONAL, -1, 1},
        {"Authorization-Lifetime", RULE_OPTIONAL, -1, 1},
        {"Auth-Grace-Period", RULE_OPTIONAL, -1, 1},
        {"Destination-Host", RULE_OPTIONAL, -1, 1},
        {"REQ-Status-Info", RULE_OPTIONAL, -1, 1},
        {"Communication-Request-Parameters", RULE_OPTIONAL, -1, 1}};
    PARSE_loc_rules(rules, cmd);

    // Answer
    data.cmd_name = "MAGIC-Client-Authentication-Answer";
    data.cmd_flag_val = CMD_FLAG_PROXIABLE;
    CHECK_dict_new(DICT_COMMAND, &data, magic_app, &cmd);

    struct local_rules_definition ans_rules[] = {
        {"Session-Id", RULE_FIXED_HEAD, -1, 1},
        {"Result-Code", RULE_REQUIRED, -1, 1},
        {"Origin-Host", RULE_REQUIRED, -1, 1},
        {"Origin-Realm", RULE_REQUIRED, -1, 1},
        {"Auth-Application-Id", RULE_REQUIRED, -1, 1},
        {"Server-Password", RULE_REQUIRED, -1, 1},
        {"Auth-Session-State", RULE_REQUIRED, -1, 1},
        {"Authorization-Lifetime", RULE_REQUIRED, -1, 1},
        {"Session-Timeout", RULE_REQUIRED, -1, 1},
        {"Auth-Grace-Period", RULE_OPTIONAL, -1, 1},
        {"Destination-Host", RULE_OPTIONAL, -1, 1},
        {"Failed-AVP", RULE_OPTIONAL, -1, 1},
        {"MAGIC-Status-Code", RULE_OPTIONAL, -1, 1},
        {"Error-Message", RULE_OPTIONAL, -1, 1},
        {"REQ-Status-Info", RULE_OPTIONAL, -1, 1},
        {"Communication-Answer-Parameters", RULE_OPTIONAL, -1, 1}};
    PARSE_loc_rules(ans_rules, cmd);
  }

  // MCCR/MCCA - MAGIC-Communication-Change (100001)
  {
    // Request
    struct dict_object *cmd;
    struct dict_cmd_data data = {
        100001,                               /* Code */
        "MAGIC-Communication-Change-Request", /* Name */
        CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE | CMD_FLAG_ERROR,
        CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE};
    CHECK_dict_new(DICT_COMMAND, &data, magic_app, &cmd);

    struct local_rules_definition rules[] = {
        {"Session-Id", RULE_FIXED_HEAD, -1, 1},
        {"Origin-Host", RULE_REQUIRED, -1, 1},
        {"Origin-Realm", RULE_REQUIRED, -1, 1},
        {"Destination-Realm", RULE_REQUIRED, -1, 1},
        {"Communication-Request-Parameters", RULE_REQUIRED, -1, 1}};
    PARSE_loc_rules(rules, cmd);

    // Answer
    data.cmd_name = "MAGIC-Communication-Change-Answer";
    data.cmd_flag_val = CMD_FLAG_PROXIABLE;
    CHECK_dict_new(DICT_COMMAND, &data, magic_app, &cmd);

    struct local_rules_definition ans_rules[] = {
        {"Session-Id", RULE_FIXED_HEAD, -1, 1},
        {"Result-Code", RULE_REQUIRED, -1, 1},
        {"Origin-Host", RULE_REQUIRED, -1, 1},
        {"Origin-Realm", RULE_REQUIRED, -1, 1},
        {"Failed-AVP", RULE_OPTIONAL, -1, 1},
        {"MAGIC-Status-Code", RULE_OPTIONAL, -1, 1},
        {"Error-Message", RULE_OPTIONAL, -1, 1},
        {"Communication-Answer-Parameters", RULE_REQUIRED, -1, 1}};
    PARSE_loc_rules(ans_rules, cmd);
  }

  // MNTR/MNTA - MAGIC-Notification (100002)
  {
    // Report (Request)
    struct dict_object *cmd;
    struct dict_cmd_data data = {100002,                      /* Code */
                                 "MAGIC-Notification-Report", /* Name */
                                 CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE |
                                     CMD_FLAG_ERROR,
                                 CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE};
    CHECK_dict_new(DICT_COMMAND, &data, magic_app, &cmd);

    struct local_rules_definition rules[] = {
        {"Session-Id", RULE_FIXED_HEAD, -1, 1},
        {"Origin-Host", RULE_REQUIRED, -1, 1},
        {"Origin-Realm", RULE_REQUIRED, -1, 1},
        {"Destination-Realm", RULE_REQUIRED, -1, 1},
        {"Communication-Report-Parameters", RULE_REQUIRED, -1, 1},
        {"MAGIC-Status-Code", RULE_OPTIONAL, -1, 1},
        {"Error-Message", RULE_OPTIONAL, -1, 1}};
    PARSE_loc_rules(rules, cmd);

    // Answer
    data.cmd_name = "MAGIC-Notification-Answer";
    data.cmd_flag_val = CMD_FLAG_PROXIABLE;
    CHECK_dict_new(DICT_COMMAND, &data, magic_app, &cmd);

    struct local_rules_definition ans_rules[] = {
        {"Session-Id", RULE_FIXED_HEAD, -1, 1},
        {"Result-Code", RULE_REQUIRED, -1, 1},
        {"Origin-Host", RULE_REQUIRED, -1, 1},
        {"Origin-Realm", RULE_REQUIRED, -1, 1},
        {"Failed-AVP", RULE_OPTIONAL, -1, 1}};
    PARSE_loc_rules(ans_rules, cmd);
  }

  // MSCR/MSCA - MAGIC-Status-Change (100003)
  {
    // Report (Request)
    struct dict_object *cmd;
    struct dict_cmd_data data = {100003,                       /* Code */
                                 "MAGIC-Status-Change-Report", /* Name */
                                 CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE |
                                     CMD_FLAG_ERROR,
                                 CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE};
    CHECK_dict_new(DICT_COMMAND, &data, magic_app, &cmd);

    struct local_rules_definition rules[] = {
        {"Session-Id", RULE_FIXED_HEAD, -1, 1},
        {"Origin-Host", RULE_REQUIRED, -1, 1},
        {"Origin-Realm", RULE_REQUIRED, -1, 1},
        {"Destination-Realm", RULE_REQUIRED, -1, 1},
        {"MAGIC-Status-Code", RULE_OPTIONAL, -1, 1},
        {"Error-Message", RULE_OPTIONAL, -1, 1},
        {"Status-Type", RULE_OPTIONAL, -1, 1},
        {"Registered-Clients", RULE_OPTIONAL, -1, 1},
        {"DLM-List", RULE_OPTIONAL, -1, 1}};
    PARSE_loc_rules(rules, cmd);

    // Answer
    data.cmd_name = "MAGIC-Status-Change-Answer";
    data.cmd_flag_val = CMD_FLAG_PROXIABLE;
    CHECK_dict_new(DICT_COMMAND, &data, magic_app, &cmd);

    struct local_rules_definition ans_rules[] = {
        {"Session-Id", RULE_FIXED_HEAD, -1, 1},
        {"Result-Code", RULE_REQUIRED, -1, 1},
        {"Origin-Host", RULE_REQUIRED, -1, 1},
        {"Origin-Realm", RULE_REQUIRED, -1, 1},
        {"Failed-AVP", RULE_OPTIONAL, -1, 1}};
    PARSE_loc_rules(ans_rules, cmd);
  }

  // MSXR/MSXA - MAGIC-Status (100004)
  {
    // Request
    struct dict_object *cmd;
    struct dict_cmd_data data = {100004,                 /* Code */
                                 "MAGIC-Status-Request", /* Name */
                                 CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE |
                                     CMD_FLAG_ERROR,
                                 CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE};
    CHECK_dict_new(DICT_COMMAND, &data, magic_app, &cmd);

    struct local_rules_definition rules[] = {
        {"Session-Id", RULE_FIXED_HEAD, -1, 1},
        {"Origin-Host", RULE_REQUIRED, -1, 1},
        {"Origin-Realm", RULE_REQUIRED, -1, 1},
        {"Destination-Realm", RULE_REQUIRED, -1, 1},
        {"Status-Type", RULE_REQUIRED, -1, 1}};
    PARSE_loc_rules(rules, cmd);

    // Answer
    data.cmd_name = "MAGIC-Status-Answer";
    data.cmd_flag_val = CMD_FLAG_PROXIABLE;
    CHECK_dict_new(DICT_COMMAND, &data, magic_app, &cmd);

    struct local_rules_definition ans_rules[] = {
        {"Session-Id", RULE_FIXED_HEAD, -1, 1},
        {"Result-Code", RULE_REQUIRED, -1, 1},
        {"Origin-Host", RULE_REQUIRED, -1, 1},
        {"Origin-Realm", RULE_REQUIRED, -1, 1},
        {"Status-Type", RULE_REQUIRED, -1, 1},
        {"MAGIC-Status-Code", RULE_OPTIONAL, -1, 1},
        {"Error-Message", RULE_OPTIONAL, -1, 1},
        {"Failed-AVP", RULE_OPTIONAL, -1, 1},
        {"Registered-Clients", RULE_OPTIONAL, -1, 1},
        {"DLM-List", RULE_OPTIONAL, -1, 1}};
    PARSE_loc_rules(ans_rules, cmd);
  }

  // MADR/MADA - MAGIC-Accounting-Data (100005)
  {
    // Request
    struct dict_object *cmd;
    struct dict_cmd_data data = {100005,                          /* Code */
                                 "MAGIC-Accounting-Data-Request", /* Name */
                                 CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE |
                                     CMD_FLAG_ERROR,
                                 CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE};
    CHECK_dict_new(DICT_COMMAND, &data, magic_app, &cmd);

    struct local_rules_definition rules[] = {
        {"Session-Id", RULE_FIXED_HEAD, -1, 1},
        {"Origin-Host", RULE_REQUIRED, -1, 1},
        {"Origin-Realm", RULE_REQUIRED, -1, 1},
        {"Destination-Realm", RULE_REQUIRED, -1, 1},
        {"CDR-Type", RULE_REQUIRED, -1, 1},
        {"CDR-Level", RULE_REQUIRED, -1, 1},
        {"CDR-Request-Identifier", RULE_OPTIONAL, -1, 1}};
    PARSE_loc_rules(rules, cmd);

    // Answer
    data.cmd_name = "MAGIC-Accounting-Data-Answer";
    data.cmd_flag_val = CMD_FLAG_PROXIABLE;
    CHECK_dict_new(DICT_COMMAND, &data, magic_app, &cmd);

    struct local_rules_definition ans_rules[] = {
        {"Session-Id", RULE_FIXED_HEAD, -1, 1},
        {"Result-Code", RULE_REQUIRED, -1, 1},
        {"Origin-Host", RULE_REQUIRED, -1, 1},
        {"Origin-Realm", RULE_REQUIRED, -1, 1},
        {"CDR-Type", RULE_REQUIRED, -1, 1},
        {"CDR-Level", RULE_REQUIRED, -1, 1},
        {"CDR-Request-Identifier", RULE_OPTIONAL, -1, 1},
        {"CDRs-Active", RULE_OPTIONAL, -1, 1},
        {"CDRs-Finished", RULE_OPTIONAL, -1, 1},
        {"CDRs-Forwarded", RULE_OPTIONAL, -1, 1},
        {"CDRs-Unknown", RULE_OPTIONAL, -1, 1},
        {"MAGIC-Status-Code", RULE_OPTIONAL, -1, 1},
        {"Error-Message", RULE_OPTIONAL, -1, 1},
        {"Failed-AVP", RULE_OPTIONAL, -1, 1}};
    PARSE_loc_rules(ans_rules, cmd);
  }

  // MACR/MACA - MAGIC-Accounting-Control (100006)
  {
    // Request
    struct dict_object *cmd;
    struct dict_cmd_data data = {100006,                             /* Code */
                                 "MAGIC-Accounting-Control-Request", /* Name */
                                 CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE |
                                     CMD_FLAG_ERROR,
                                 CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE};
    CHECK_dict_new(DICT_COMMAND, &data, magic_app, &cmd);

    struct local_rules_definition rules[] = {
        {"Session-Id", RULE_FIXED_HEAD, -1, 1},
        {"Origin-Host", RULE_REQUIRED, -1, 1},
        {"Origin-Realm", RULE_REQUIRED, -1, 1},
        {"CDR-Restart-Session-Id", RULE_REQUIRED, -1, 1}};
    PARSE_loc_rules(rules, cmd);

    // Answer
    data.cmd_name = "MAGIC-Accounting-Control-Answer";
    data.cmd_flag_val = CMD_FLAG_PROXIABLE;
    CHECK_dict_new(DICT_COMMAND, &data, magic_app, &cmd);

    struct local_rules_definition ans_rules[] = {
        {"Session-Id", RULE_FIXED_HEAD, -1, 1},
        {"Result-Code", RULE_REQUIRED, -1, 1},
        {"Origin-Host", RULE_REQUIRED, -1, 1},
        {"Origin-Realm", RULE_REQUIRED, -1, 1},
        {"CDR-Restart-Session-Id", RULE_REQUIRED, -1, 1},
        {"MAGIC-Status-Code", RULE_OPTIONAL, -1, 1},
        {"Error-Message", RULE_OPTIONAL, -1, 1},
        {"Failed-AVP", RULE_OPTIONAL, -1, 1},
        {"CDRs-Updated", RULE_OPTIONAL, -1, 1}};
    PARSE_loc_rules(ans_rules, cmd);
  }

  // 6. 注册应用支持 (Application Support)
  //    告诉 freeDiameter 此 peer 支持 MAGIC Application
  //    这样在 CER/CEA 握手时会声明支持的应用 ID
  //    客户端和服务端都需要这个声明才能建立连接
  {
    CHECK_FCT(fd_disp_app_support(magic_app, magic_vendor, 1, 0));
    fd_log_notice("[dict_magic_839] Registered MAGIC Application support "
                  "(App-ID: 1094202169, Vendor-ID: 13712)");
  }

  return 0;
}

EXTENSION_ENTRY("dict_magic_839", dict_magic_arinc839_entry);