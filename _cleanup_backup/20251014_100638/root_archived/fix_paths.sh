#!/bin/bash

# 批量修正配置文件中的硬编码路径
echo "开始修正配置文件中的硬编码路径..."

# 获取所有包含硬编码路径的配置文件
CONFIG_FILES=$(find /home/zhuwuhui/freeDiameter -name "*.conf" -exec grep -l "/home/zhuwuhui" {} \;)

for file in $CONFIG_FILES; do
    echo "处理文件: $file"
    
    # 备份原文件（如果还没有备份）
    if [ ! -f "$file.backup" ]; then
        cp "$file" "$file.backup"
    fi
    
    # 根据文件所在目录确定相对路径
    if [[ "$file" == *"/diameter_client/"* ]]; then
        # diameter_client 目录下的文件
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/client\.crt|client.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/client\.key|client.key|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/ca\.crt|ca.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/server\.crt|server.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/server\.key|server.key|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/dict_arinc839\.fdx|../build/extensions/dict_arinc839.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/server\.fdx|../build/extensions/server.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/dbg_msg_dumps\.fdx|../build/extensions/dbg_msg_dumps.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/rt_default\.fdx|../build/extensions/rt_default.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/acl_wl\.fdx|../build/extensions/acl_wl.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/rt_default\.conf|../build/rt_default.conf|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/acl_wl\.conf|../build/acl_wl.conf|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/server\.crt|../build/server.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/server\.key|../build/server.key|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/ca\.crt|../build/ca.crt|g' "$file"
    elif [[ "$file" == *"/exe/"* ]] || [[ "$file" == *"/exe_client/"* ]]; then
        # exe 目录下的文件
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/client\.crt|certs/client.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/client\.key|certs/client.key|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/ca\.crt|certs/ca.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/server\.crt|certs/server.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/server\.key|certs/server.key|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/exe/certs/client\.crt|certs/client.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/exe/certs/client\.key|certs/client.key|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/exe/certs/ca\.crt|certs/ca.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/exe/certs/server\.crt|certs/server.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/exe/certs/server\.key|certs/server.key|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/dict_arinc839\.fdx|dict_arinc839.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/server\.fdx|server.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/dbg_msg_dumps\.fdx|dbg_msg_dumps.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/rt_default\.fdx|rt_default.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/acl_wl\.fdx|acl_wl.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/rt_default\.conf|rt_default.conf|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/acl_wl\.conf|acl_wl.conf|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/server\.crt|certs/server.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/server\.key|certs/server.key|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/ca\.crt|certs/ca.crt|g' "$file"
    elif [[ "$file" == *"/build/"* ]]; then
        # build 目录下的文件
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/client\.crt|../diameter_client/client.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/client\.key|../diameter_client/client.key|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/ca\.crt|../diameter_client/ca.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/server\.crt|../diameter_client/server.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/server\.key|../diameter_client/server.key|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/dict_arinc839\.fdx|extensions/dict_arinc839.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/server\.fdx|extensions/server.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/dbg_msg_dumps\.fdx|extensions/dbg_msg_dumps.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/rt_default\.fdx|extensions/rt_default.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/acl_wl\.fdx|extensions/acl_wl.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/rt_default\.conf|rt_default.conf|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/acl_wl\.conf|acl_wl.conf|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/server\.crt|server.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/server\.key|server.key|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/ca\.crt|ca.crt|g' "$file"
    else
        # 其他目录下的文件，使用相对路径
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/client\.crt|../diameter_client/client.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/client\.key|../diameter_client/client.key|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/ca\.crt|../diameter_client/ca.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/server\.crt|../diameter_client/server.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/diameter_client/server\.key|../diameter_client/server.key|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/dict_arinc839\.fdx|../build/extensions/dict_arinc839.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/server\.fdx|../build/extensions/server.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/dbg_msg_dumps\.fdx|../build/extensions/dbg_msg_dumps.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/rt_default\.fdx|../build/extensions/rt_default.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/extensions/acl_wl\.fdx|../build/extensions/acl_wl.fdx|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/rt_default\.conf|../build/rt_default.conf|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/acl_wl\.conf|../build/acl_wl.conf|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/server\.crt|../build/server.crt|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/server\.key|../build/server.key|g' "$file"
        sed -i 's|/home/zhuwuhui/freeDiameter/build/ca\.crt|../build/ca.crt|g' "$file"
    fi
    
    echo "  已处理: $file"
done

echo "路径修正完成！"
echo "原始文件已备份为 .backup 后缀"