# 更新日志 (CHANGELOG)

## 2026-04-18

### 重构：所有裸指针替换为智能指针
- 使用 `std::unique_ptr` 替代所有裸指针，自动管理内存
- `TaskParam::func_pattern` 改为 `std::vector<std::unique_ptr<FuncPattern>>`
- `ModTsFile` 构造函数接收 `std::unique_ptr<TaskParam>` 转移所有权
- `ModTsFile` 内部所有 pattern 列表使用 `std::unique_ptr`
- `TsPAT::parsePAT`、`TsPMT::parsePMT` 等返回 `std::unique_ptr`
- `pat_`、`pmt_pair_list_` 使用智能指针管理
- 移除所有手动 `delete` 代码，消除内存泄漏风险
- Makefile 升级为 C++14 以支持 `std::make_unique`

### 新增 PID 直接指定模式
- `FuncPattern` 类新增 `target_pid` 字段，支持直接指定 PID 进行修改
- JSON 配置支持可选 `"pid"` 字段，指定后直接按 PID 匹配，无需 PMT 解析
- 修改 `getPtsChange`、`shouldDeleteThisPack`、`shouldFillWithNull` 函数支持 PID 匹配
- 新增 `hasPidPattern` 辅助函数检查是否有 pattern 指定该 PID
- 即使未解析 PAT/PMT，指定 PID 的 pattern 也能生效

**用法示例：**
```json
{
  "pattern": [
    {
      "start_sec": 30,
      "end_sec": 90,
      "pid": 256,
      "func": "add",
      "pts_base": 180000
    }
  ]
}
```

### 修复日志目录不存在时程序崩溃问题
- `createLogFile` 先创建父目录 `m_logPath`，再创建月份子目录
- `initLogLevel` 时主动创建日志文件，避免首次写入时警告

## 2026-04-17

### 更新.gitignore过滤日志和临时文件
- 添加 `test.json`、`input.srt`、`*.srt` 等临时测试文件过滤
- 删除已提交的日志文件 `log/202404/202404.log`

### 增强TS解析健壮性和改进帮助文档

**TS解析修复：**
- PAT/PMT 解析添加缓冲区边界检查，防止越界访问
- `threadRunning` 添加同步字节验证 (0x47) 和读取完整性检查
- 修复 `cur_position` 越界问题（adaptation_field 长度检查）
- 空包 PID 改为标准 `0x1FFF`
- 重复包更新连续计数器，避免解码器报错

**帮助文档改进：**
- `makejson.py` 无参数时显示帮助，`-i` 参数必需
- `mod_tsfile` 帮助输出改进，清晰展示用法和操作说明

### 优化代码结构和修复潜在问题

**命名修正：**
- `TS_PACHET_LENGTH` → `TS_PACKET_LENGTH`
- `ModeTsFileStatus` → `ModTsFileStatus`
- `changeOutoutFile` → `changeOutputFile`

**内存管理优化：**
- `TsPacketHeader` 和 `OptionalPesHeader` 改用栈上对象避免频繁 new/delete
- 初始化 `cut_param_` 为 nullptr 防止未初始化崩溃

**代码质量：**
- 合并重复代码：添加 `getStreamTypeByPid()` 辅助函数
- 增强错误处理：文件打开和 JSON 解析失败后正确退出
- 消除未使用变量和编译警告

### 修复代码缺陷并添加Makefile

**代码缺陷修复：**
- `parse_json.cpp` 错误消息 `"Missing 'func'"` → `"Missing 'pts_base'"`
- `parse_json.cpp` `pat_delete/pmt_delete` 返回正确字符串而非 "null"
- `mod_ts_file.cpp` 赋值/比较混淆 `statu_ == Stop` → `statu_ = Stop`
- `mod_ts_file.cpp` `changeDTS` 减法处理与 `changePTS` 保持一致
- `mod_ts_file.cpp` 清理调试输出，统一使用 Log 系统
- `main.cpp` JSON 访问顺序错误，先检查 `HasMember` 再访问
- `main.cpp` `releaseTasks` 开头的 return 阻断代码执行
- `Log.cpp` `createLogFile` 添加返回值

**新增 Makefile：**
- 替代 shell 脚本，支持增量编译
- `make` 编译，`make clean` 清理

---

## 使用说明

### 编译
```bash
make
```

### 生成配置文件
```bash
python3 makejson.py -c 264 -i input.ts -o config.json
python3 makejson.py -c 265 -i input.ts -o config.json
```

### 运行
```bash
./mod_tsfile config.json
```

### PTS偏移量计算
```
pts_base = 秒数 × 90000 (90kHz时钟)

常用值：
- 0.5秒 = 45000
- 1秒   = 90000
- 2秒   = 180000
- 5秒   = 450000
```