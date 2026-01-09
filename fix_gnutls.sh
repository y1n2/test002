#!/bin/bash

# 修改 include/freeDiameter/CMakeLists.txt 文件
echo "Modifying include/freeDiameter/CMakeLists.txt..."
sed -i 's/SET(GNUTLS_INCLUDE_DIR "" PARENT_SCOPE)/SET(GNUTLS_INCLUDE_DIR "\/usr\/include\/gnutls" PARENT_SCOPE)/' /home/zhuwuhui/freeDiameter/include/freeDiameter/CMakeLists.txt

echo "Done!"