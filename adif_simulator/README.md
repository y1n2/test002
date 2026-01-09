ADIF 模拟器使用说明

概述
----
ADIF Simulator（Aircraft Data Interface Function Simulator）用于模拟飞机航电参数并通过 TCP 向订阅客户端推送 XML 格式数据。
该程序基于项目中的 `adif_simulator.c`，提供交互式 CLI，可手动或自动生成飞行状态数据，便于与 MAGIC 系统或其他测试客户端集成。

关键点：
```text

手动控制示例（完整流程、分步骤）
---------------------------------
下面给出一组完整的手动命令序列，演示如何从登机口手动推进到着陆并返回登机口，适用于在交互式 `adif>` 提示下逐条执行。

准备：在独立终端启动模拟器：

```bash
# 启动模拟器（默认端口 4000）
/home/zhuwuhui/freeDiameter/adif_simulator/build/adif_simulator
```

示例步骤（在 `adif>` 提示下手动执行）：

1) 登机口（GATE）

```text
adif> tail N12345
adif> pos 33.9425 -118.4081 120
adif> speed 0 0
adif> wow 1
adif> phase GATE
adif> status
```

2) 滑行（TAXI）

```text
adif> phase TAXI
adif> speed 15 0
adif> pos 33.9430 -118.4070 120
adif> status
```

3) 跑道并起飞（TAKEOFF）

```text
adif> phase TAKEOFF
adif> speed 160 0
adif> wow 0        # 轮子离地
adif> pos 33.9440 -118.4050 500
adif> status
```

4) 爬升（CLIMB） — 分步更新位置和高度

```text
adif> phase CLIMB
adif> pos 33.9450 -118.4020 2000
adif> speed 220 2000
adif> pos 33.9500 -118.3900 10000
adif> speed 300 1500
adif> status
```

5) 巡航（CRUISE）

```text
adif> phase CRUISE
adif> pos 35.0000 -115.0000 35000
adif> speed 450 0
adif> status
```

6) 下降与进近（DESCENT → APPROACH）

```text
adif> phase DESCENT
adif> pos 34.5000 -118.2000 25000
adif> speed 350 -1500
adif> phase APPROACH
adif> pos 33.9600 -118.3800 3000
adif> speed 160 -800
adif> status
```

7) 着陆并滑行回登机口（LANDING → TAXI → GATE）

```text
adif> phase LANDING
adif> pos 33.9440 -118.4050 120
adif> speed 100 0
adif> wow 1
adif> phase TAXI
adif> speed 15 0
adif> pos 33.9430 -118.4070 120
adif> phase GATE
adif> speed 0 0
adif> status
```

说明：上面各步均可按需在 `adif>` 中逐条执行并查看 `status` 输出。若想模拟连续移动，可以在外部 shell 中每隔若干秒通过 `expect` 或脚本向模拟器的 stdin 发送命令，但默认交互式手动输入更直观。

订阅与推送（仅用命令行工具接收）
--------------------------------
下面示例说明如何使用纯命令行工具（`nc` / `ncat` / `socat`）完成订阅并接收异步推送：

# 终端 A: 启动模拟器（listen 4000）
/home/zhuwuhui/freeDiameter/adif_simulator/build/adif_simulator

# 终端 B: 启动本地监听，等待模拟器连接（接收异步 XML 推送）
```
# 推荐（保持连接）：ncat -l -k 64001
# 或常见 netcat：nc -l 64001
# 或使用 socat：socat - TCP-LISTEN:64001,reuseaddr
```

# 终端 C: 发送订阅请求到模拟器（确保终端 B 已在监听）
```bash
printf '<method name="subscribeAvionicParameters"><parameters>\n  <parameter name="publishport" value="64001"/>\n  <parameter name="refreshperiod" value="1000"/>\n</parameters></method>' | nc localhost 4000
```

行为说明：
- 订阅请求会在终端 C 的连接上收到 `subscribeAvionicParametersResponse`。
- 模拟器随后会连接回终端 B 的 `64001` 端口，并按 `refreshperiod` 周期推送 XML。

示例推送片段（终端 B 会看到类似内容）：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<method name="publishAvionicParameters">
    <parameters>
        <parameter name="WeightOnWheels" value="1" validity="1" time="1680000000000"/>
        <parameter name="Latitude" value="33.942500" validity="1" time="1680000000000"/>
        <parameter name="Longitude" value="-118.408100" validity="1" time="1680000000000"/>
        <parameter name="BaroCorrectedAltitude" value="120" validity="1" time="1680000000000"/>
        ...
    </parameters>
</method>
```

调试与自动/批量手动控制建议
-----------------------------
- 若想按脚本批量发送手动命令，可以用 `expect` 或简单的 shell 脚本把命令写入 `adif_simulator` 的 stdin。但请注意：程序为交互式 readline，会在脚本模式下行为略有差异。
- 若需要频繁复现同一序列，建议把每步命令写到一个文本文件，然后在一个交互式会话中使用 `rlwrap` 或 `expect` 来重放。
- 若模拟器没有连接回客户端，请检查客户端监听端口是否允许外部连接（防火墙、本地绑定等）。


ASYNC_PORT = 64001
ADIF_HOST = '127.0.0.1'
ADIF_PORT = 4000

# 监听线程：接收异步推送
def listener():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('0.0.0.0', ASYNC_PORT))
    s.listen(1)
    print('Listening for async pushes on', ASYNC_PORT)
    conn, addr = s.accept()
    print('Connected by', addr)
    while True:
        data = conn.recv(8192)
        if not data:
            break
        print('PUSH> ', data.decode(errors='ignore'))
    conn.close()

# 主线程：发送订阅请求
def main():
    t = threading.Thread(target=listener, daemon=True)
    t.start()

    sub_xml = ('<method name="subscribeAvionicParameters"><parameters>'
               '<parameter name="publishport" value="%d"/>'
               '<parameter name="refreshperiod" value="1000"/>'
               '</parameters></method>') % ASYNC_PORT

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((ADIF_HOST, ADIF_PORT))
    s.send(sub_xml.encode())
    resp = s.recv(4096)
    print('RESP> ', resp.decode(errors='ignore'))
    s.close()

    # keep process alive to receive pushes
    t.join()

if __name__ == '__main__':
    main()
```

常见问题与排查
--------------
- 无法连接或推送：检查防火墙和端口是否打开。
- `nc` 无法持续显示数据：某些 `nc` 版本会在连接断开后退出，建议使用 `ncat -l -k` 或运行 Python 接收示例。
- 构建失败提示缺少 `readline`：请安装开发包，例如在 Debian/Ubuntu 上运行 `sudo apt-get install libreadline-dev`。

与 MAGIC 集成小贴士
------------------
- MAGIC 的 `magic_adif` 模块可以作为订阅客户端连接到 ADIF 模拟器，确保其配置中订阅请求包含正确的 `publishport`（即模拟器会回连的端口）。
- 在集成测试中，先在 MAGIC 端配置并启动本地监听，再在 ADIF 端发起订阅，或按上文 Python 示例先启动本地监听再发送订阅。

版权与贡献
-----------
该文件为项目内部使用文档，可根据需要扩展示例和脚本。如果需要我可以为您添加一个配套的 Python 客户端脚本到该目录中。
