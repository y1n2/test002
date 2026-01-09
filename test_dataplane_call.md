# 数据平面问题诊断总结

## 问题现象

1. **ipsetWhitelist为空**：
   - `magic_control`: 0 entries  
   - `magic_data`: 0 entries

2. **iptables mangle 表无规则**：
   - PREROUTING 链完全为空

3. **路由表配置错误**：
   - 表100 应该指向 ens33 (SATCOM) 却指向 ens38
   - 表101 应该指向 ens38 (WIFI) 却指向 ens33

## 根本原因分析

根据代码审查，发现以下可能性：

### 可能性 1：客户端 IP 获取失败

在 `magic_cic.c:1150` 行，代码尝试获取客户端 IP：

```c
const char *client_ip = NULL;
if (ctx->profile && ctx->profile->auth.source_ip[0]) {
    client_ip = ctx->profile->auth.source_ip;
} else if (ctx->session && ctx->session->client_ip[0]) {
    client_ip = ctx->session->client_ip;
} else {
    client_ip = "192.168.10.10"; /* 默认 */
}
```

**问题**：客户端实际 IP 是 `192.168.126.5`，但可能被设为了默认值 `192.168.10.10`。

### 可能性 2：数据平面未初始化

检查 `enable_routing` 标志，如果为 false，则所有数据平面操作会被跳过。

### 可能性 3：链路注册失败

`magic_dataplane_register_link()` 可能在没有接口信息的情况下失败，导致后续路由规则无法添加。

## 立即验证步骤

1. 重启服务器并观察日志
2. 检查 Client_Profile.xml 中的 SourceIP 配置
3. 手动添加 iptables 规则测试数据平面架构
4. 检查 LMI 链路信息是否正确注册

