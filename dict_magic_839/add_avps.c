/**
 * @file add_avps.c
 * @brief MAGIC ARINC 839-2014 AVP 定义代码。
 * @details 此文件由 csv_to_fd 工具自动生成，定义了 MAGIC 协议中使用的所有基础
 * AVP。 包括 AVP 编码、厂商 ID、数据类型（如 Enumerated,
 * UTF8String）以及标志位。
 *
 * @warning 请勿手动修改此文件，除非确定不再运行生成脚本。
 * @author AEEC / ARINC (Generated)
 */

#include <freeDiameter/extension.h>

#define CHECK_dict_new(_type, _data, _parent, _ref)                            \
  CHECK_FCT(fd_dict_new(fd_g_config->cnf_dict, (_type), (_data), (_parent),    \
                        (_ref)));

#define CHECK_dict_search(_type, _criteria, _what, _result)                    \
  CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, (_type), (_criteria),        \
                           (_what), (_result), ENOENT));

/**
 * @brief 注册 ARINC 839-2014 定义的所有基础 AVP。
 * @details 负责向 freeDiameter 词典系统加载 AVP、子类型及枚举值。
 *          涵盖范围：Client-Password (10001) 到 MAGIC-Status-Code (10053) 等。
 *
 * @return int 成功返回 0，失败返回错误码。
 * @note 此函数被 dict_magic_arinc839_entry 调用。
 */
int add_avps() {
  struct dict_object *UTF8String_type = NULL;
  CHECK_dict_search(DICT_TYPE, TYPE_BY_NAME, "UTF8String", &UTF8String_type);

  /*==================================================================*/
  /* ARINC 839-2014 MAGIC Diameter AVPs                               */
  /*==================================================================*/

  /* Basic AVPs (10001-10053)                                         */

  /* Client-Password, UTF8String, code 10001, section 1.1.1.2.1       */
  {
    struct dict_avp_data data = {
        10001,                                /* Code */
        13712,                                /* Vendor */
        "Client-Password",                    /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING /* base type of data (derived as UTF8String) */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* REQ-Status-Info, Enumerated, code 10002, section 1.1.1.3.1       */
  {
    struct dict_object *type;
    struct dict_type_data tdata = {
        AVP_TYPE_UNSIGNED32, "Enumerated(REQ-Status-Info)", NULL, NULL, NULL};
    struct dict_enumval_data t_0 = {"No_Status", {.u32 = 0}};
    struct dict_enumval_data t_1 = {"MAGIC_Status", {.u32 = 1}};
    struct dict_enumval_data t_2 = {"MIHF_Status", {.u32 = 2}};
    struct dict_enumval_data t_3 = {"MAGIC_DLM_Status", {.u32 = 3}};
    struct dict_enumval_data t_4 = {"Policy_Engine_Status", {.u32 = 4}};
    struct dict_enumval_data t_5 = {"System_Status", {.u32 = 5}};
    struct dict_enumval_data t_6 = {"Session_Status", {.u32 = 6}};
    struct dict_enumval_data t_7 = {"MAGIC_DLM_LINK_Status", {.u32 = 7}};

    struct dict_avp_data data = {
        10002,                                /* Code */
        13712,                                /* Vendor */
        "REQ-Status-Info",                    /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_UNSIGNED32                   /* base type of data */
    };

    /* Create the Enumerated type, and then the AVP */
    CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
    CHECK_dict_new(DICT_ENUMVAL, &t_0, type, NULL);
    CHECK_dict_new(DICT_ENUMVAL, &t_1, type, NULL);
    CHECK_dict_new(DICT_ENUMVAL, &t_2, type, NULL);
    CHECK_dict_new(DICT_ENUMVAL, &t_3, type, NULL);
    CHECK_dict_new(DICT_ENUMVAL, &t_4, type, NULL);
    CHECK_dict_new(DICT_ENUMVAL, &t_5, type, NULL);
    CHECK_dict_new(DICT_ENUMVAL, &t_6, type, NULL);
    CHECK_dict_new(DICT_ENUMVAL, &t_7, type, NULL);
    CHECK_dict_new(DICT_AVP, &data, type, NULL);
  };

  /* Status-Type, Enumerated, code 10003, section 1.1.1.3.2           */
  {
    struct dict_object *type;
    struct dict_type_data tdata = {AVP_TYPE_UNSIGNED32,
                                   "Enumerated(Status-Type)", NULL, NULL, NULL};
    struct dict_enumval_data t_0 = {"No_Status", {.u32 = 0}};
    struct dict_enumval_data t_1 = {"MAGIC_Status", {.u32 = 1}};
    struct dict_enumval_data t_2 = {"MIHF_Status", {.u32 = 2}};
    struct dict_enumval_data t_3 = {"MAGIC_DLM_Status", {.u32 = 3}};
    struct dict_enumval_data t_4 = {"Policy_Engine_Status", {.u32 = 4}};
    struct dict_enumval_data t_5 = {"System_Status", {.u32 = 5}};
    struct dict_enumval_data t_6 = {"Session_Status", {.u32 = 6}};
    struct dict_enumval_data t_7 = {"MAGIC_DLM_LINK_Status", {.u32 = 7}};

    struct dict_avp_data data = {
        10003,                                /* Code */
        13712,                                /* Vendor */
        "Status-Type",                        /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_UNSIGNED32                   /* base type of data */
    };

    /* Create the Enumerated type, and then the AVP */
    CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
    CHECK_dict_new(DICT_ENUMVAL, &t_0, type, NULL);
    CHECK_dict_new(DICT_ENUMVAL, &t_1, type, NULL);
    CHECK_dict_new(DICT_ENUMVAL, &t_2, type, NULL);
    CHECK_dict_new(DICT_ENUMVAL, &t_3, type, NULL);
    CHECK_dict_new(DICT_ENUMVAL, &t_4, type, NULL);
    CHECK_dict_new(DICT_ENUMVAL, &t_5, type, NULL);
    CHECK_dict_new(DICT_ENUMVAL, &t_6, type, NULL);
    CHECK_dict_new(DICT_ENUMVAL, &t_7, type, NULL);
    CHECK_dict_new(DICT_AVP, &data, type, NULL);
  };

  /* DLM-Name, UTF8String, code 10004, section 1.1.1.4.1              */
  {
    struct dict_avp_data data = {
        10004,                                /* Code */
        13712,                                /* Vendor */
        "DLM-Name",                           /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* DLM-Available, Enumerated, code 10005, section 1.1.1.4.2         */
  {
    struct dict_avp_data data = {
        10005,                                /* Code */
        13712,                                /* Vendor */
        "DLM-Available",                      /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_INTEGER32                    /* base type of data */
    };
    struct dict_object *type;
    struct dict_type_data tdata = {
        AVP_TYPE_INTEGER32, "Enumerated(AEEC/DLM-Available)", NULL, NULL, NULL};
    CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
    CHECK_dict_new(DICT_AVP, &data, type, NULL);
  };

  /* DLM-Max-Bandwidth, Float32, code 10006, section 1.1.1.4.3        */
  {
    struct dict_avp_data data = {
        10006,                                /* Code */
        13712,                                /* Vendor */
        "DLM-Max-Bandwidth",                  /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_FLOAT32                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* DLM-Allocated-Bandwidth, Float32, code 10007, section 1.1.1.4.4  */
  {
    struct dict_avp_data data = {
        10007,                                /* Code */
        13712,                                /* Vendor */
        "DLM-Allocated-Bandwidth",            /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_FLOAT32                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* DLM-Max-Return-Bandwidth, Float32, code 10008, section 1.1.1.4.5 */
  {
    struct dict_avp_data data = {
        10008,                                /* Code */
        13712,                                /* Vendor */
        "DLM-Max-Return-Bandwidth",           /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_FLOAT32                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* DLM-Allocated-Return-Bandwidth, Float32, code 10009, section 1.1.1.4.6 */
  {
    struct dict_avp_data data = {
        10009,                                /* Code */
        13712,                                /* Vendor */
        "DLM-Allocated-Return-Bandwidth",     /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_FLOAT32                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* DLM-Max-Links, Unsigned32, code 10010, section 1.1.1.4.7         */
  {
    struct dict_avp_data data = {
        10010,                                /* Code */
        13712,                                /* Vendor */
        "DLM-Max-Links",                      /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_UNSIGNED32                   /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* DLM-Allocated-Links, Unsigned32, code 10011, section 1.1.1.4.8   */
  {
    struct dict_avp_data data = {
        10011,                                /* Code */
        13712,                                /* Vendor */
        "DLM-Allocated-Links",                /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_UNSIGNED32                   /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Link-Number, Unsigned32, code 10012, section 1.1.1.5.1           */
  {
    struct dict_avp_data data = {
        10012,                                /* Code */
        13712,                                /* Vendor */
        "Link-Number",                        /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_UNSIGNED32                   /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Link-Available, Enumerated, code 10013, section 1.1.1.5.3        */
  {
    struct dict_avp_data data = {
        10013,                                /* Code */
        13712,                                /* Vendor */
        "Link-Available",                     /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_INTEGER32                    /* base type of data */
    };
    struct dict_object *type;
    struct dict_type_data tdata = {AVP_TYPE_INTEGER32,
                                   "Enumerated(AEEC/Link-Available)", NULL,
                                   NULL, NULL};
    CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
    CHECK_dict_new(DICT_AVP, &data, type, NULL);
  };

  /* Link-Connection-Status, Enumerated, code 10014, section 1.1.1.5.4 */
  {
    struct dict_avp_data data = {
        10014,                                /* Code */
        13712,                                /* Vendor */
        "Link-Connection-Status",             /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_INTEGER32                    /* base type of data */
    };
    struct dict_object *type;
    struct dict_type_data tdata = {AVP_TYPE_INTEGER32,
                                   "Enumerated(AEEC/Link-Connection-Status)",
                                   NULL, NULL, NULL};
    CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
    CHECK_dict_new(DICT_AVP, &data, type, NULL);
  };

  /* Link-Login-Status, Enumerated, code 10015, section 1.1.1.5.5     */
  {
    struct dict_avp_data data = {
        10015,                                /* Code */
        13712,                                /* Vendor */
        "Link-Login-Status",                  /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_INTEGER32                    /* base type of data */
    };
    struct dict_object *type;
    struct dict_type_data tdata = {AVP_TYPE_INTEGER32,
                                   "Enumerated(AEEC/Link-Login-Status)", NULL,
                                   NULL, NULL};
    CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
    CHECK_dict_new(DICT_AVP, &data, type, NULL);
  };

  /* Link-Max-Bandwidth, Float32, code 10016, section 1.1.1.5.6       */
  {
    struct dict_avp_data data = {
        10016,                                /* Code */
        13712,                                /* Vendor */
        "Link-Max-Bandwidth",                 /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_FLOAT32                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Link-Max-Return-Bandwidth, Float32, code 10017, section 1.1.1.5.7 */
  {
    struct dict_avp_data data = {
        10017,                                /* Code */
        13712,                                /* Vendor */
        "Link-Max-Return-Bandwidth",          /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_FLOAT32                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Link-Alloc-Bandwidth, Float32, code 10018, section 1.1.1.5.8     */
  {
    struct dict_avp_data data = {
        10018,                                /* Code */
        13712,                                /* Vendor */
        "Link-Alloc-Bandwidth",               /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_FLOAT32                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Link-Alloc-Return-Bandwidth, Float32, code 10019, section 1.1.1.5.9 */
  {
    struct dict_avp_data data = {
        10019,                                /* Code */
        13712,                                /* Vendor */
        "Link-Alloc-Return-Bandwidth",        /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_FLOAT32                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Link-Error-String, UTF8String, code 10020, section 1.1.1.5.10    */
  {
    struct dict_avp_data data = {
        10020,                                /* Code */
        13712,                                /* Vendor */
        "Link-Error-String",                  /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* Requested-Bandwidth, Float32, code 10021, section 1.1.1.6.1.1    */
  {
    struct dict_avp_data data = {
        10021,                                /* Code */
        13712,                                /* Vendor */
        "Requested-Bandwidth",                /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_FLOAT32                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Requested-Return-Bandwidth, Float32, code 10022, section 1.1.1.6.1.2 */
  {
    struct dict_avp_data data = {
        10022,                                /* Code */
        13712,                                /* Vendor */
        "Requested-Return-Bandwidth",         /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_FLOAT32                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Required-Bandwidth, Float32, code 10023, section 1.1.1.6.1.3     */
  {
    struct dict_avp_data data = {
        10023,                                /* Code */
        13712,                                /* Vendor */
        "Required-Bandwidth",                 /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_FLOAT32                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Required-Return-Bandwidth, Float32, code 10024, section 1.1.1.6.1.4 */
  {
    struct dict_avp_data data = {
        10024,                                /* Code */
        13712,                                /* Vendor */
        "Required-Return-Bandwidth",          /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_FLOAT32                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Priority-Class, UTF8String, code 10025, section 1.1.1.6.2.1      */
  {
    struct dict_avp_data data = {
        10025,                                /* Code */
        13712,                                /* Vendor */
        "Priority-Class",                     /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* Priority-Type, Enumerated, code 10026, section 1.1.1.6.2.2       */
  {
    struct dict_avp_data data = {
        10026,                                /* Code */
        13712,                                /* Vendor */
        "Priority-Type",                      /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_INTEGER32                    /* base type of data */
    };
    struct dict_object *type;
    struct dict_type_data tdata = {
        AVP_TYPE_INTEGER32, "Enumerated(AEEC/Priority-Type)", NULL, NULL, NULL};
    CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
    CHECK_dict_new(DICT_AVP, &data, type, NULL);
  };

  /* QoS-Level, Enumerated, code 10027, section 1.1.1.6.2.3           */
  {
    struct dict_avp_data data = {
        10027,                                /* Code */
        13712,                                /* Vendor */
        "QoS-Level",                          /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_INTEGER32                    /* base type of data */
    };
    struct dict_object *type;
    struct dict_type_data tdata = {
        AVP_TYPE_INTEGER32, "Enumerated(AEEC/QoS-Level)", NULL, NULL, NULL};
    CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
    CHECK_dict_new(DICT_AVP, &data, type, NULL);
  };

  /* DLM-Availability-List, UTF8String, code 10028, section 1.1.1.6.3.1 */
  {
    struct dict_avp_data data = {
        10028,                                /* Code */
        13712,                                /* Vendor */
        "DLM-Availability-List",              /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* Gateway-IPAddress, UTF8String, code 10029, section 1.1.1.6.3.2   */
  {
    struct dict_avp_data data = {
        10029,                                /* Code */
        13712,                                /* Vendor */
        "Gateway-IPAddress",                  /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* TFTtoGround-Rule, UTF8String, code 10030, section 1.1.1.6.3.3.1  */
  {
    struct dict_avp_data data = {
        10030,                                /* Code */
        13712,                                /* Vendor */
        "TFTtoGround-Rule",                   /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* TFTtoAircraft-Rule, UTF8String, code 10031, section 1.1.1.6.3.3.2 */
  {
    struct dict_avp_data data = {
        10031,                                /* Code */
        13712,                                /* Vendor */
        "TFTtoAircraft-Rule",                 /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* NAPT-Rule, UTF8String, code 10032, section 1.1.1.6.3.3.3         */
  {
    struct dict_avp_data data = {
        10032,                                /* Code */
        13712,                                /* Vendor */
        "NAPT-Rule",                          /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* Flight-Phase, UTF8String, code 10033, section 1.1.1.6.4.1        */
  {
    struct dict_avp_data data = {
        10033,                                /* Code */
        13712,                                /* Vendor */
        "Flight-Phase",                       /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* Altitude, UTF8String, code 10034, section 1.1.1.6.4.2            */
  {
    struct dict_avp_data data = {
        10034,                                /* Code */
        13712,                                /* Vendor */
        "Altitude",                           /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* Airport, UTF8String, code 10035, section 1.1.1.6.4.3             */
  {
    struct dict_avp_data data = {
        10035,                                /* Code */
        13712,                                /* Vendor */
        "Airport",                            /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* Accounting-Enabled, Unsigned32, code 10036, section 1.1.1.6.5.1  */
  {
    struct dict_avp_data data = {
        10036,                                /* Code */
        13712,                                /* Vendor */
        "Accounting-Enabled",                 /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_UNSIGNED32                   /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Keep-Request, Enumerated, code 10037, section 1.1.1.6.5.2        */
  {
    struct dict_avp_data data = {
        10037,                                /* Code */
        13712,                                /* Vendor */
        "Keep-Request",                       /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_INTEGER32                    /* base type of data */
    };
    struct dict_object *type;
    struct dict_type_data tdata = {
        AVP_TYPE_INTEGER32, "Enumerated(AEEC/Keep-Request)", NULL, NULL, NULL};
    CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
    CHECK_dict_new(DICT_AVP, &data, type, NULL);
  };

  /* Auto-Detect, Enumerated, code 10038, section 1.1.1.6.5.3         */
  {
    struct dict_avp_data data = {
        10038,                                /* Code */
        13712,                                /* Vendor */
        "Auto-Detect",                        /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_INTEGER32                    /* base type of data */
    };
    struct dict_object *type;
    struct dict_type_data tdata = {
        AVP_TYPE_INTEGER32, "Enumerated(AEEC/Auto-Detect)", NULL, NULL, NULL};
    CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
    CHECK_dict_new(DICT_AVP, &data, type, NULL);
  };

  /* Timeout, Unsigned32, code 10039, section 1.1.1.6.5.4             */
  {
    struct dict_avp_data data = {
        10039,                                /* Code */
        13712,                                /* Vendor */
        "Timeout",                            /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_UNSIGNED32                   /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Profile-Name, UTF8String, code 10040, section 1.1.1.7.1          */
  {
    struct dict_avp_data data = {
        10040,                                /* Code */
        13712,                                /* Vendor */
        "Profile-Name",                       /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* Registered-Clients, UTF8String, code 10041, section 1.1.1.7.2    */
  {
    struct dict_avp_data data = {
        10041,                                /* Code */
        13712,                                /* Vendor */
        "Registered-Clients",                 /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* CDR-Type, Enumerated, code 10042, section 1.1.1.8.1.1            */
  {
    struct dict_avp_data data = {
        10042,                                /* Code */
        13712,                                /* Vendor */
        "CDR-Type",                           /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_INTEGER32                    /* base type of data */
    };
    struct dict_object *type;
    struct dict_type_data tdata = {
        AVP_TYPE_INTEGER32, "Enumerated(AEEC/CDR-Type)", NULL, NULL, NULL};
    CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
    CHECK_dict_new(DICT_AVP, &data, type, NULL);
  };

  /* CDR-Level, Enumerated, code 10043, section 1.1.1.8.1.2           */
  {
    struct dict_avp_data data = {
        10043,                                /* Code */
        13712,                                /* Vendor */
        "CDR-Level",                          /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_INTEGER32                    /* base type of data */
    };
    struct dict_object *type;
    struct dict_type_data tdata = {
        AVP_TYPE_INTEGER32, "Enumerated(AEEC/CDR-Level)", NULL, NULL, NULL};
    CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
    CHECK_dict_new(DICT_AVP, &data, type, NULL);
  };

  /* CDR-Request-Identifier, UTF8String, code 10044, section 1.1.1.8.1.3 */
  {
    struct dict_avp_data data = {
        10044,                                /* Code */
        13712,                                /* Vendor */
        "CDR-Request-Identifier",             /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* Server-Password, UTF8String, code 10045, section 1.1.1.2.1       */
  {
    struct dict_avp_data data = {
        10045,                                /* Code */
        13712,                                /* Vendor */
        "Server-Password",                    /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* CDR-ID, Unsigned32, code 10046, section 1.1.1.8.1.4              */
  {
    struct dict_avp_data data = {
        10046,                                /* Code */
        13712,                                /* Vendor */
        "CDR-ID",                             /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_UNSIGNED32                   /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* CDR-Content, UTF8String, code 10047, section 1.1.1.8.1.5         */
  {
    struct dict_avp_data data = {
        10047,                                /* Code */
        13712,                                /* Vendor */
        "CDR-Content",                        /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* CDR-Restart-Session-Id, UTF8String, code 10048, section 1.1.1.8.2.1 */
  {
    struct dict_avp_data data = {
        10048,                                /* Code */
        13712,                                /* Vendor */
        "CDR-Restart-Session-Id",             /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* CDR-Stopped, Unsigned32, code 10049, section 1.1.1.8.2.2         */
  {
    struct dict_avp_data data = {
        10049,                                /* Code */
        13712,                                /* Vendor */
        "CDR-Stopped",                        /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_UNSIGNED32                   /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* CDR-Started, Unsigned32, code 10050, section 1.1.1.8.2.3         */
  {
    struct dict_avp_data data = {
        10050,                                /* Code */
        13712,                                /* Vendor */
        "CDR-Started",                        /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_UNSIGNED32                   /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Granted-Bandwidth, Float32, code 10051, section 1.1.1.6.1.5      */
  {
    struct dict_avp_data data = {
        10051,                                /* Code */
        13712,                                /* Vendor */
        "Granted-Bandwidth",                  /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_FLOAT32                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Granted-Return-Bandwidth, Float32, code 10052, section 1.1.1.6.1.6 */
  {
    struct dict_avp_data data = {
        10052,                                /* Code */
        13712,                                /* Vendor */
        "Granted-Return-Bandwidth",           /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_FLOAT32                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* MAGIC-Status-Code, Unsigned32, code 10053, section 1.1.1.9.2     */
  {
    struct dict_avp_data data = {
        10053,                                /* Code */
        13712,                                /* Vendor */
        "MAGIC-Status-Code",                  /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_UNSIGNED32                   /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Link-Name, UTF8String, code 10054, section 1.1.1.5.2             */
  {
    struct dict_avp_data data = {
        10054,                                /* Code */
        13712,                                /* Vendor */
        "Link-Name",                          /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_OCTETSTRING                  /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
  };

  /* Grouped AVPs (20001-20019)                                       */

  /* Communication-Request-Parameters, Grouped, code 20001, section 1.1.2.1.1 */
  {
    struct dict_avp_data data = {
        20001,                                /* Code */
        13712,                                /* Vendor */
        "Communication-Request-Parameters",   /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Communication-Answer-Parameters, Grouped, code 20002, section 1.1.2.1.2 */
  {
    struct dict_avp_data data = {
        20002,                                /* Code */
        13712,                                /* Vendor */
        "Communication-Answer-Parameters",    /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Communication-Report-Parameters, Grouped, code 20003, section 1.1.2.1.3 */
  {
    struct dict_avp_data data = {
        20003,                                /* Code */
        13712,                                /* Vendor */
        "Communication-Report-Parameters",    /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* TFTtoGround-List, Grouped, code 20004, section 1.1.2.2.1         */
  {
    struct dict_avp_data data = {
        20004,                                /* Code */
        13712,                                /* Vendor */
        "TFTtoGround-List",                   /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* TFTtoAircraft-List, Grouped, code 20005, section 1.1.2.2.2       */
  {
    struct dict_avp_data data = {
        20005,                                /* Code */
        13712,                                /* Vendor */
        "TFTtoAircraft-List",                 /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* NAPT-List, Grouped, code 20006, section 1.1.2.2.3                */
  {
    struct dict_avp_data data = {
        20006,                                /* Code */
        13712,                                /* Vendor */
        "NAPT-List",                          /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* DLM-List, Grouped, code 20007, section 1.1.2.3.1                 */
  {
    struct dict_avp_data data = {
        20007,                                /* Code */
        13712,                                /* Vendor */
        "DLM-List",                           /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* DLM-Info, Grouped, code 20008, section 1.1.2.3.2                 */
  {
    struct dict_avp_data data = {
        20008,                                /* Code */
        13712,                                /* Vendor */
        "DLM-Info",                           /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* DLM-QoS-Level-List, Grouped, code 20009, section 1.1.2.3.3       */
  {
    struct dict_avp_data data = {
        20009,                                /* Code */
        13712,                                /* Vendor */
        "DLM-QoS-Level-List",                 /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* DLM-Link-Status-List, Grouped, code 20010, section 1.1.2.3.4     */
  {
    struct dict_avp_data data = {
        20010,                                /* Code */
        13712,                                /* Vendor */
        "DLM-Link-Status-List",               /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Link-Status-Group, Grouped, code 20011, section 1.1.2.3.5        */
  {
    struct dict_avp_data data = {
        20011,                                /* Code */
        13712,                                /* Vendor */
        "Link-Status-Group",                  /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* CDRs-Active, Grouped, code 20012, section 1.1.2.4.1              */
  {
    struct dict_avp_data data = {
        20012,                                /* Code */
        13712,                                /* Vendor */
        "CDRs-Active",                        /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* CDRs-Finished, Grouped, code 20013, section 1.1.2.4.2            */
  {
    struct dict_avp_data data = {
        20013,                                /* Code */
        13712,                                /* Vendor */
        "CDRs-Finished",                      /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* CDRs-Forwarded, Grouped, code 20014, section 1.1.2.4.3           */
  {
    struct dict_avp_data data = {
        20014,                                /* Code */
        13712,                                /* Vendor */
        "CDRs-Forwarded",                     /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* CDRs-Unknown, Grouped, code 20015, section 1.1.2.4.4             */
  {
    struct dict_avp_data data = {
        20015,                                /* Code */
        13712,                                /* Vendor */
        "CDRs-Unknown",                       /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* CDRs-Updated, Grouped, code 20016, section 1.1.2.4.5             */
  {
    struct dict_avp_data data = {
        20016,                                /* Code */
        13712,                                /* Vendor */
        "CDRs-Updated",                       /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* CDR-Info, Grouped, code 20017, section 1.1.2.4.6                 */
  {
    struct dict_avp_data data = {
        20017,                                /* Code */
        13712,                                /* Vendor */
        "CDR-Info",                           /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* CDR-Start-Stop-Pair, Grouped, code 20018, section 1.1.2.4.7      */
  {
    struct dict_avp_data data = {
        20018,                                /* Code */
        13712,                                /* Vendor */
        "CDR-Start-Stop-Pair",                /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /* Client-Credentials, Grouped, code 20019, section 1.1.2.5.1       */
  {
    struct dict_avp_data data = {
        20019,                                /* Code */
        13712,                                /* Vendor */
        "Client-Credentials",                 /* Name */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flags */
        AVP_FLAG_VENDOR | AVP_FLAG_MANDATORY, /* Fixed flag values */
        AVP_TYPE_GROUPED                      /* base type of data */
    };
    CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
  };

  /*==================================================================*/
  /* Enumeration Values for MAGIC AVPs                                */
  /*==================================================================*/

  /* DLM-Available (10005) */
  {
    struct dict_object *type;
    struct dict_enumval_data val;
    CHECK_dict_search(DICT_TYPE, TYPE_BY_NAME, "Enumerated(AEEC/DLM-Available)",
                      &type);

    val.enum_value.i32 = 1;
    val.enum_name = "YES";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);

    val.enum_value.i32 = 2;
    val.enum_name = "NO";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);

    val.enum_value.i32 = 3;
    val.enum_name = "UNKNOWN";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);
  }

  /* Link-Available (10013) */
  {
    struct dict_object *type;
    struct dict_enumval_data val;
    CHECK_dict_search(DICT_TYPE, TYPE_BY_NAME,
                      "Enumerated(AEEC/Link-Available)", &type);

    val.enum_value.i32 = 1;
    val.enum_name = "YES";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);

    val.enum_value.i32 = 2;
    val.enum_name = "NO";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);
  }

  /* Link-Connection-Status (10014) */
  {
    struct dict_object *type;
    struct dict_enumval_data val;
    CHECK_dict_search(DICT_TYPE, TYPE_BY_NAME,
                      "Enumerated(AEEC/Link-Connection-Status)", &type);

    val.enum_value.i32 = 1;
    val.enum_name = "Disconnected";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);

    val.enum_value.i32 = 2;
    val.enum_name = "Connected";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);

    val.enum_value.i32 = 3;
    val.enum_name = "Forced_Close";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);
  }

  /* Link-Login-Status (10015) */
  {
    struct dict_object *type;
    struct dict_enumval_data val;
    CHECK_dict_search(DICT_TYPE, TYPE_BY_NAME,
                      "Enumerated(AEEC/Link-Login-Status)", &type);

    val.enum_value.i32 = 1;
    val.enum_name = "Logged_off";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);

    val.enum_value.i32 = 2;
    val.enum_name = "Logged_on";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);
  }

  /* Priority-Type (10026) */
  {
    struct dict_object *type;
    struct dict_enumval_data val;
    CHECK_dict_search(DICT_TYPE, TYPE_BY_NAME, "Enumerated(AEEC/Priority-Type)",
                      &type);

    val.enum_value.i32 = 1;
    val.enum_name = "Blocking";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);

    val.enum_value.i32 = 2;
    val.enum_name = "Preemption";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);
  }

  /* QoS-Level (10027) */
  {
    struct dict_object *type;
    struct dict_enumval_data val;
    CHECK_dict_search(DICT_TYPE, TYPE_BY_NAME, "Enumerated(AEEC/QoS-Level)",
                      &type);

    val.enum_value.i32 = 0;
    val.enum_name = "BE"; /* Best Effort */
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);

    val.enum_value.i32 = 1;
    val.enum_name = "AF"; /* Assured Forwarding */
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);

    val.enum_value.i32 = 2;
    val.enum_name = "EF"; /* Expedited Forwarding */
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);
  }

  /* Keep-Request (10037) */
  {
    struct dict_object *type;
    struct dict_enumval_data val;
    CHECK_dict_search(DICT_TYPE, TYPE_BY_NAME, "Enumerated(AEEC/Keep-Request)",
                      &type);

    val.enum_value.i32 = 0;
    val.enum_name = "NO";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);

    val.enum_value.i32 = 1;
    val.enum_name = "YES";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);
  }

  /* Auto-Detect (10038) */
  {
    struct dict_object *type;
    struct dict_enumval_data val;
    CHECK_dict_search(DICT_TYPE, TYPE_BY_NAME, "Enumerated(AEEC/Auto-Detect)",
                      &type);

    val.enum_value.i32 = 0;
    val.enum_name = "NO";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);

    val.enum_value.i32 = 1;
    val.enum_name = "YES_Symmetric";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);

    val.enum_value.i32 = 2;
    val.enum_name = "YES_Asymmetric";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);
  }

  /* CDR-Type (10042) */
  {
    struct dict_object *type;
    struct dict_enumval_data val;
    CHECK_dict_search(DICT_TYPE, TYPE_BY_NAME, "Enumerated(AEEC/CDR-Type)",
                      &type);

    val.enum_value.i32 = 1;
    val.enum_name = "LIST_REQUEST";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);

    val.enum_value.i32 = 2;
    val.enum_name = "DATA_REQUEST";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);
  }

  /* CDR-Level (10043) */
  {
    struct dict_object *type;
    struct dict_enumval_data val;
    CHECK_dict_search(DICT_TYPE, TYPE_BY_NAME, "Enumerated(AEEC/CDR-Level)",
                      &type);

    val.enum_value.i32 = 1;
    val.enum_name = "ALL";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);

    val.enum_value.i32 = 2;
    val.enum_name = "USER_DEPENDENT";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);

    val.enum_value.i32 = 3;
    val.enum_name = "SESSION_DEPENDENT";
    CHECK_dict_new(DICT_ENUMVAL, &val, type, NULL);
  }

  return 0;
} /* add_avps() */
