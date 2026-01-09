/**
 * @file xml_config_parser.c
 * @brief MAGIC XML 配置文件解析器实现
 */

#include "xml_config_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

/*===========================================================================
 * 辅助函数：智能路径查找
 * 说明：按优先级尝试多个路径，找到第一个存在的文件
 *===========================================================================*/

static const char* find_config_file(const char* filename, char* result_path, size_t path_len)
{
    // 尝试的路径列表（按优先级）
    const char* search_paths[] = {
        "../config/%s",              // 从 build/ 目录运行
        "config/%s",                 // 当前目录有 config/ 子目录
        "./%s",                      // 当前目录
        "magic_server/config/%s",    // 从项目根目录运行
        "/home/zhuwuhui/freeDiameter/magic_server/config/%s",  // 绝对路径
        NULL
    };
    
    for (int i = 0; search_paths[i] != NULL; i++) {
        snprintf(result_path, path_len, search_paths[i], filename);
        
        // 检查文件是否存在
        FILE* test = fopen(result_path, "r");
        if (test) {
            fclose(test);
            return result_path;  // 找到文件
        }
    }
    
    return NULL;  // 未找到文件
}

/*===========================================================================
 * 辅助函数：XML 节点解析
 *===========================================================================*/

static char* get_node_content(xmlNode* node)
{
    if (node && node->children && node->children->content) {
        return (char*)node->children->content;
    }
    return NULL;
}

static xmlNode* find_child_node(xmlNode* parent, const char* name)
{
    if (!parent) return NULL;
    
    for (xmlNode* node = parent->children; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE && 
            strcmp((char*)node->name, name) == 0) {
            return node;
        }
    }
    return NULL;
}

static char* get_child_content(xmlNode* parent, const char* child_name)
{
    xmlNode* child = find_child_node(parent, child_name);
    return get_node_content(child);
}

static uint32_t get_child_uint32(xmlNode* parent, const char* child_name, uint32_t default_val)
{
    char* content = get_child_content(parent, child_name);
    return content ? (uint32_t)atoi(content) : default_val;
}

static char* get_attribute(xmlNode* node, const char* attr_name)
{
    return (char*)xmlGetProp(node, (const xmlChar*)attr_name);
}

/*===========================================================================
 * 枚举类型转换
 *===========================================================================*/

static LinkType parse_link_type(const char* str)
{
    if (!str) return LINK_TYPE_UNKNOWN;
    if (strcmp(str, "SATELLITE") == 0) return LINK_TYPE_SATELLITE;
    if (strcmp(str, "CELLULAR") == 0) return LINK_TYPE_CELLULAR;
    if (strcmp(str, "GATELINK") == 0) return LINK_TYPE_GATELINK;
    return LINK_TYPE_UNKNOWN;
}

static Coverage parse_coverage(const char* str)
{
    if (!str) return COVERAGE_UNKNOWN;
    if (strcmp(str, "GLOBAL") == 0) return COVERAGE_GLOBAL;
    if (strcmp(str, "TERRESTRIAL") == 0) return COVERAGE_TERRESTRIAL;
    if (strcmp(str, "GATE_ONLY") == 0) return COVERAGE_GATE_ONLY;
    return COVERAGE_UNKNOWN;
}

static SecurityLevel parse_security_level(const char* str)
{
    if (!str) return SECURITY_NONE;
    if (strcmp(str, "HIGH") == 0) return SECURITY_HIGH;
    if (strcmp(str, "MEDIUM") == 0) return SECURITY_MEDIUM;
    if (strcmp(str, "LOW") == 0) return SECURITY_LOW;
    return SECURITY_NONE;
}

static AuthenticationType parse_auth_type(const char* str)
{
    if (!str) return AUTH_UNKNOWN;
    if (strcmp(str, "MAGIC_AWARE") == 0) return AUTH_MAGIC_AWARE;
    if (strcmp(str, "NON_AWARE") == 0) return AUTH_NON_AWARE;
    return AUTH_UNKNOWN;
}

static PolicyAction parse_policy_action(const char* str)
{
    if (!str) return ACTION_DEFAULT;
    if (strcmp(str, "PERMIT") == 0) return ACTION_PERMIT;
    if (strcmp(str, "PROHIBIT") == 0) return ACTION_PROHIBIT;
    return ACTION_DEFAULT;
}

/*===========================================================================
 * 初始化
 *===========================================================================*/

void magic_config_init(MagicConfig* config)
{
    memset(config, 0, sizeof(MagicConfig));
    config->is_loaded = false;
}

/*===========================================================================
 * 解析 Datalink_Profile.xml
 *===========================================================================*/

int magic_config_load_datalinks(MagicConfig* config, const char* filepath)
{
    char actual_path[512];
    const char* found_path = find_config_file("Datalink_Profile.xml", actual_path, sizeof(actual_path));
    
    if (!found_path) {
        fprintf(stderr, "[CONFIG] Error: Cannot find Datalink_Profile.xml\n");
        fprintf(stderr, "[CONFIG] Searched in: ../config/, config/, ./, magic_server/config/\n");
        return -1;
    }
    
    printf("[CONFIG] Loading Datalink Profile: %s\n", found_path);
    
    xmlDoc* doc = xmlReadFile(found_path, NULL, 0);
    if (!doc) {
        fprintf(stderr, "[CONFIG] Error: Failed to parse %s\n", found_path);
        return -1;
    }
    
    xmlNode* root = xmlDocGetRootElement(doc);
    if (!root || strcmp((char*)root->name, "DatalinkProfiles") != 0) {
        fprintf(stderr, "[CONFIG] Error: Invalid root element\n");
        xmlFreeDoc(doc);
        return -1;
    }
    
    config->num_datalinks = 0;
    
    // 遍历所有 <Link> 节点
    for (xmlNode* link_node = root->children; link_node; link_node = link_node->next) {
        if (link_node->type != XML_ELEMENT_NODE || 
            strcmp((char*)link_node->name, "Link") != 0) {
            continue;
        }
        
        if (config->num_datalinks >= MAX_LINKS) {
            fprintf(stderr, "[CONFIG] Warning: Too many links, skipping\n");
            break;
        }
        
        DatalinkProfile* link = &config->datalinks[config->num_datalinks];
        memset(link, 0, sizeof(DatalinkProfile));
        
        // 解析 id 属性
        char* id = get_attribute(link_node, "id");
        if (id) {
            strncpy(link->link_id, id, MAX_ID_LEN - 1);
            xmlFree(id);
        }
        
        // 解析基本信息
        char* name = get_child_content(link_node, "LinkName");
        if (name) strncpy(link->link_name, name, MAX_NAME_LEN - 1);
        
        char* dlm_id = get_child_content(link_node, "DLMDriverID");
        if (dlm_id) strncpy(link->dlm_driver_id, dlm_id, MAX_ID_LEN - 1);
        
        char* iface = get_child_content(link_node, "InterfaceName");
        if (iface) strncpy(link->interface_name, iface, 15);
        
        char* type_str = get_child_content(link_node, "Type");
        link->type = parse_link_type(type_str);
        
        // 解析 Capabilities
        xmlNode* cap_node = find_child_node(link_node, "Capabilities");
        if (cap_node) {
            link->capabilities.max_tx_rate_kbps = 
                get_child_uint32(cap_node, "MaxTxRateKbps", 0);
            link->capabilities.typical_latency_ms = 
                get_child_uint32(cap_node, "TypicalLatencyMs", 0);
        }
        
        // 解析 PolicyAttributes
        xmlNode* policy_node = find_child_node(link_node, "PolicyAttributes");
        if (policy_node) {
            link->policy_attrs.cost_index = 
                get_child_uint32(policy_node, "CostIndex", 50);
            
            char* sec = get_child_content(policy_node, "SecurityLevel");
            link->policy_attrs.security_level = parse_security_level(sec);
            
            char* cov = get_child_content(policy_node, "Coverage");
            link->policy_attrs.coverage = parse_coverage(cov);
        }
        
        link->is_active = false;
        config->num_datalinks++;
        
        printf("[CONFIG]   Loaded link: %s (%s)\n", link->link_id, link->link_name);
    }
    
    xmlFreeDoc(doc);
    printf("[CONFIG] Loaded %u datalinks\n", config->num_datalinks);
    return 0;
}

/*===========================================================================
 * 解析 Central_Policy_Profile.xml
 *===========================================================================*/

int magic_config_load_policy(MagicConfig* config, const char* filepath)
{
    char actual_path[512];
    const char* found_path = find_config_file("Central_Policy_Profile.xml", actual_path, sizeof(actual_path));
    
    if (!found_path) {
        fprintf(stderr, "[CONFIG] Error: Cannot find Central_Policy_Profile.xml\n");
        return -1;
    }
    
    printf("[CONFIG] Loading Policy Profile: %s\n", found_path);
    
    xmlDoc* doc = xmlReadFile(found_path, NULL, 0);
    if (!doc) {
        fprintf(stderr, "[CONFIG] Error: Failed to parse %s\n", found_path);
        return -1;
    }
    
    xmlNode* root = xmlDocGetRootElement(doc);
    if (!root || strcmp((char*)root->name, "CentralPolicyProfile") != 0) {
        fprintf(stderr, "[CONFIG] Error: Invalid root element\n");
        xmlFreeDoc(doc);
        return -1;
    }
    
    CentralPolicyProfile* policy = &config->policy;
    memset(policy, 0, sizeof(CentralPolicyProfile));
    
    // 解析 AvailableLinks
    xmlNode* links_node = find_child_node(root, "AvailableLinks");
    if (links_node) {
        policy->num_links = 0;
        for (xmlNode* link = links_node->children; link; link = link->next) {
            if (link->type == XML_ELEMENT_NODE && 
                strcmp((char*)link->name, "Link") == 0 &&
                policy->num_links < MAX_LINKS) {
                
                char* id = get_attribute(link, "id");
                if (id) {
                    strncpy(policy->available_links[policy->num_links], 
                            id, MAX_ID_LEN - 1);
                    policy->num_links++;
                    xmlFree(id);
                }
            }
        }
    }
    
    // 解析 PolicyRuleSet
    policy->num_rulesets = 0;
    for (xmlNode* ruleset_node = root->children; ruleset_node; 
         ruleset_node = ruleset_node->next) {
        
        if (ruleset_node->type != XML_ELEMENT_NODE || 
            strcmp((char*)ruleset_node->name, "PolicyRuleSet") != 0) {
            continue;
        }
        
        if (policy->num_rulesets >= MAX_POLICY_RULESETS) {
            fprintf(stderr, "[CONFIG] Warning: Too many rulesets\n");
            break;
        }
        
        PolicyRuleSet* ruleset = &policy->rulesets[policy->num_rulesets];
        memset(ruleset, 0, sizeof(PolicyRuleSet));
        
        // 解析属性
        char* id = get_attribute(ruleset_node, "id");
        if (id) {
            strncpy(ruleset->ruleset_id, id, MAX_ID_LEN - 1);
            xmlFree(id);
        }
        
        char* phases = get_attribute(ruleset_node, "flight_phases");
        if (phases) {
            strncpy(ruleset->flight_phases, phases, MAX_NAME_LEN - 1);
            xmlFree(phases);
        }
        
        // 解析 PolicyRule
        ruleset->num_rules = 0;
        for (xmlNode* rule_node = ruleset_node->children; rule_node; 
             rule_node = rule_node->next) {
            
            if (rule_node->type != XML_ELEMENT_NODE || 
                strcmp((char*)rule_node->name, "PolicyRule") != 0) {
                continue;
            }
            
            if (ruleset->num_rules >= MAX_RULES_PER_RULESET) {
                break;
            }
            
            PolicyRule* rule = &ruleset->rules[ruleset->num_rules];
            memset(rule, 0, sizeof(PolicyRule));
            
            // 解析 traffic_class 属性
            char* traffic_class = get_attribute(rule_node, "traffic_class");
            if (traffic_class) {
                strncpy(rule->traffic_class, traffic_class, MAX_ID_LEN - 1);
                xmlFree(traffic_class);
            }
            
            // 解析 PathPreference
            rule->num_preferences = 0;
            for (xmlNode* pref_node = rule_node->children; pref_node; 
                 pref_node = pref_node->next) {
                
                if (pref_node->type != XML_ELEMENT_NODE || 
                    strcmp((char*)pref_node->name, "PathPreference") != 0) {
                    continue;
                }
                
                if (rule->num_preferences >= MAX_PATH_PREFERENCES) {
                    break;
                }
                
                PathPreference* pref = &rule->preferences[rule->num_preferences];
                memset(pref, 0, sizeof(PathPreference));
                
                // 解析属性
                char* ranking_str = get_attribute(pref_node, "ranking");
                if (ranking_str) {
                    pref->ranking = (uint32_t)atoi(ranking_str);
                    xmlFree(ranking_str);
                }
                
                char* link_id = get_attribute(pref_node, "link_id");
                if (link_id) {
                    strncpy(pref->link_id, link_id, MAX_ID_LEN - 1);
                    xmlFree(link_id);
                }
                
                char* action_str = get_attribute(pref_node, "action");
                pref->action = parse_policy_action(action_str);
                if (action_str) xmlFree(action_str);
                
                char* sec_req = get_attribute(pref_node, "security_required");
                if (sec_req) {
                    strncpy(pref->security_required, sec_req, 31);
                    xmlFree(sec_req);
                }
                
                rule->num_preferences++;
            }
            
            ruleset->num_rules++;
        }
        
        policy->num_rulesets++;
        printf("[CONFIG]   Loaded ruleset: %s (%u rules)\n", 
               ruleset->ruleset_id, ruleset->num_rules);
    }
    
    xmlFreeDoc(doc);
    printf("[CONFIG] Loaded %u policy rulesets\n", policy->num_rulesets);
    return 0;
}

/*===========================================================================
 * 解析 Client_Profile.xml
 *===========================================================================*/

int magic_config_load_clients(MagicConfig* config, const char* filepath)
{
    char actual_path[512];
    const char* found_path = find_config_file("Client_Profile.xml", actual_path, sizeof(actual_path));
    
    if (!found_path) {
        fprintf(stderr, "[CONFIG] Error: Cannot find Client_Profile.xml\n");
        return -1;
    }
    
    printf("[CONFIG] Loading Client Profile: %s\n", found_path);
    
    xmlDoc* doc = xmlReadFile(found_path, NULL, 0);
    if (!doc) {
        fprintf(stderr, "[CONFIG] Error: Failed to parse %s\n", found_path);
        return -1;
    }
    
    xmlNode* root = xmlDocGetRootElement(doc);
    if (!root || strcmp((char*)root->name, "ClientProfiles") != 0) {
        fprintf(stderr, "[CONFIG] Error: Invalid root element\n");
        xmlFreeDoc(doc);
        return -1;
    }
    
    config->num_clients = 0;
    
    // 遍历所有 <Client> 节点
    for (xmlNode* client_node = root->children; client_node; 
         client_node = client_node->next) {
        
        if (client_node->type != XML_ELEMENT_NODE || 
            strcmp((char*)client_node->name, "Client") != 0) {
            continue;
        }
        
        if (config->num_clients >= MAX_CLIENTS) {
            fprintf(stderr, "[CONFIG] Warning: Too many clients\n");
            break;
        }
        
        ClientProfile* client = &config->clients[config->num_clients];
        memset(client, 0, sizeof(ClientProfile));
        
        // 解析 id 属性
        char* id = get_attribute(client_node, "id");
        if (id) {
            strncpy(client->client_id, id, MAX_ID_LEN - 1);
            xmlFree(id);
        }
        
        // 解析 Authentication
        xmlNode* auth_node = find_child_node(client_node, "Authentication");
        if (auth_node) {
            char* auth_type = get_attribute(auth_node, "type");
            client->auth.type = parse_auth_type(auth_type);
            if (auth_type) xmlFree(auth_type);
            
            if (client->auth.type == AUTH_MAGIC_AWARE) {
                char* username = get_child_content(auth_node, "Username");
                if (username) strncpy(client->auth.username, username, MAX_ID_LEN - 1);
                
                char* password = get_child_content(auth_node, "Password");
                if (password) strncpy(client->auth.password, password, MAX_ID_LEN - 1);
                
                char* key = get_child_content(auth_node, "PrimaryKey");
                if (key) strncpy(client->auth.primary_key, key, MAX_ID_LEN - 1);
            }
        }
        
        // 解析 IPPortFilter (NON_AWARE)
        xmlNode* filter_node = find_child_node(client_node, "IPPortFilter");
        if (filter_node) {
            char* src_ip = get_child_content(filter_node, "SourceIpAddress");
            if (src_ip) strncpy(client->auth.source_ip, src_ip, MAX_IP_STR_LEN - 1);
            
            char* dest = get_child_content(filter_node, "DestinationIpPort");
            if (dest) strncpy(client->auth.dest_ip_port, dest, MAX_IP_STR_LEN - 1);
            
            char* ports = get_child_content(filter_node, "DestinationPortList");
            if (ports) strncpy(client->auth.dest_port_list, ports, MAX_PORTLIST_LEN - 1);
        }
        
        // 解析 Metadata
        xmlNode* meta_node = find_child_node(client_node, "Metadata");
        if (meta_node) {
            char* hw = get_child_content(meta_node, "HardwareType");
            if (hw) strncpy(client->metadata.hardware_type, hw, MAX_ID_LEN - 1);
            
            char* role = get_child_content(meta_node, "SystemRole");
            if (role) strncpy(client->metadata.system_role, role, MAX_ID_LEN - 1);
            
            char* app_id = get_child_content(meta_node, "AircraftApplicationID");
            if (app_id) strncpy(client->metadata.aircraft_app_id, app_id, MAX_ID_LEN - 1);
        }
        
        // 解析 Limits
        xmlNode* limits_node = find_child_node(client_node, "Limits");
        if (limits_node) {
            client->limits.max_session_bw_kbps = 
                get_child_uint32(limits_node, "MaxSessionBandwidthKbps", 0);
            client->limits.total_client_bw_kbps = 
                get_child_uint32(limits_node, "TotalClientBandwidthKbps", 0);
            client->limits.max_concurrent_sessions = 
                get_child_uint32(limits_node, "MaxConcurrentSessions", 1);
        }
        
        // 解析 PolicyMapping
        xmlNode* mapping_node = find_child_node(client_node, "PolicyMapping");
        if (mapping_node) {
            char* traffic_class = get_child_content(mapping_node, "TrafficClassID");
            if (traffic_class) {
                strncpy(client->traffic_class_id, traffic_class, MAX_ID_LEN - 1);
            }
        }
        
        client->is_online = false;
        config->num_clients++;
        
        printf("[CONFIG]   Loaded client: %s (%s)\n", 
               client->client_id, client->metadata.system_role);
    }
    
    xmlFreeDoc(doc);
    printf("[CONFIG] Loaded %u clients\n", config->num_clients);
    return 0;
}

/*===========================================================================
 * 加载所有配置
 *===========================================================================*/

int magic_config_load_all(MagicConfig* config)
{
    printf("\n");
    printf("========================================\n");
    printf("  Loading MAGIC Configuration Files\n");
    printf("========================================\n\n");
    
    // 初始化 libxml2
    LIBXML_TEST_VERSION
    
    // 加载数据链路配置
    if (magic_config_load_datalinks(config, DATALINK_PROFILE_FILE) != 0) {
        return -1;
    }
    
    printf("\n");
    
    // 加载策略配置
    if (magic_config_load_policy(config, POLICY_PROFILE_FILE) != 0) {
        return -1;
    }
    
    printf("\n");
    
    // 加载客户端配置
    if (magic_config_load_clients(config, CLIENT_PROFILE_FILE) != 0) {
        return -1;
    }
    
    config->load_time = time(NULL);
    config->is_loaded = true;
    
    printf("\n");
    printf("========================================\n");
    printf("  Configuration Loaded Successfully\n");
    printf("  Total: %u links, %u rulesets, %u clients\n",
           config->num_datalinks, config->policy.num_rulesets, config->num_clients);
    printf("========================================\n\n");
    
    // 清理 libxml2
    xmlCleanupParser();
    
    return 0;
}

/*===========================================================================
 * 查找函数
 *===========================================================================*/

DatalinkProfile* magic_config_find_datalink(MagicConfig* config, const char* link_id)
{
    for (uint32_t i = 0; i < config->num_datalinks; i++) {
        if (strcmp(config->datalinks[i].link_id, link_id) == 0) {
            return &config->datalinks[i];
        }
    }
    return NULL;
}

ClientProfile* magic_config_find_client(MagicConfig* config, const char* client_id)
{
    for (uint32_t i = 0; i < config->num_clients; i++) {
        if (strcmp(config->clients[i].client_id, client_id) == 0) {
            return &config->clients[i];
        }
    }
    return NULL;
}

PolicyRuleSet* magic_config_find_ruleset(MagicConfig* config, const char* flight_phase)
{
    for (uint32_t i = 0; i < config->policy.num_rulesets; i++) {
        PolicyRuleSet* ruleset = &config->policy.rulesets[i];
        if (strstr(ruleset->flight_phases, flight_phase) != NULL) {
            return ruleset;
        }
    }
    return NULL;
}

/*===========================================================================
 * 打印配置摘要
 *===========================================================================*/

void magic_config_print_summary(const MagicConfig* config)
{
    printf("\n========================================\n");
    printf("  MAGIC Configuration Summary\n");
    printf("========================================\n\n");
    
    // 数据链路
    printf("Data Links (%u):\n", config->num_datalinks);
    for (uint32_t i = 0; i < config->num_datalinks; i++) {
        const DatalinkProfile* link = &config->datalinks[i];
        printf("  [%u] %s\n", i + 1, link->link_id);
        printf("      Name:      %s\n", link->link_name);
        printf("      Interface: %s\n", link->interface_name);
        printf("      Bandwidth: %u kbps\n", link->capabilities.max_tx_rate_kbps);
        printf("      Latency:   %u ms\n", link->capabilities.typical_latency_ms);
        printf("      Cost:      %u\n", link->policy_attrs.cost_index);
        printf("\n");
    }
    
    // 策略规则集
    printf("Policy Rulesets (%u):\n", config->policy.num_rulesets);
    for (uint32_t i = 0; i < config->policy.num_rulesets; i++) {
        const PolicyRuleSet* ruleset = &config->policy.rulesets[i];
        printf("  [%u] %s\n", i + 1, ruleset->ruleset_id);
        printf("      Phases: %s\n", ruleset->flight_phases);
        printf("      Rules:  %u\n", ruleset->num_rules);
        printf("\n");
    }
    
    // 客户端
    printf("Clients (%u):\n", config->num_clients);
    for (uint32_t i = 0; i < config->num_clients; i++) {
        const ClientProfile* client = &config->clients[i];
        printf("  [%u] %s\n", i + 1, client->client_id);
        printf("      Role:     %s\n", client->metadata.system_role);
        printf("      Auth:     %s\n", 
               client->auth.type == AUTH_MAGIC_AWARE ? "MAGIC_AWARE" : "NON_AWARE");
        printf("      Bandwidth: %u kbps\n", client->limits.total_client_bw_kbps);
        printf("\n");
    }
    
    printf("========================================\n\n");
}

/*===========================================================================
 * 清理
 *===========================================================================*/

void magic_config_cleanup(MagicConfig* config)
{
    if (config) {
        memset(config, 0, sizeof(MagicConfig));
    }
}
