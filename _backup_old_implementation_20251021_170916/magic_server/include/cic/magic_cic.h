#ifndef _MAGIC_CIC_H
#define _MAGIC_CIC_H

#include "../magic_common.h"

/* 客户端接口控制器初始化 */
int cic_init(char *conffile);

/* 客户端接口控制器清理 */
void cic_cleanup(void);

#endif /* _MAGIC_CIC_H */