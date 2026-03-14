use std::collections::BTreeMap;
use std::env;
use std::fs::File;
use std::io::{self, BufRead, BufReader};

fn main() -> io::Result<()> {
    // 我现在 Lua 1.1 已经能用 LUA_TRACE=1 打印 VM 每条指令了，
    // 但是 trace 一多就会刷屏，看着很累。
    //
    // 所以我干脆写个 Rust 小程序：
    //   - 把 trace 输出读进来
    //   - 把每条 opcode 名字统计一下出现次数
    //   - 最后把统计结果打印出来
    //
    // 这样我就能很直观地看到：我这个脚本到底主要在跑哪些指令。

    // 用法 1：从文件读（适合我先把 trace 保存起来）
    //   cargo run -- /tmp/trace.log
    //
    // 用法 2：从管道读（适合我边跑 Lua 边统计）
    //   LUA_TRACE=1 ./bin/lua /tmp/t.lua 2>&1 | cargo run
    //
    // 注意：Lua 的 trace 是打到 stderr 的，所以我在命令里用了 2>&1，
    // 把 stderr 合并到 stdout，不然 Rust 这边可能读不到。

    let args: Vec<String> = env::args().collect();

    // - 如果我运行时带了一个参数，那就当成文件路径
    // - 如果没带参数，就从标准输入读（也就是管道喂进来的内容）
    let reader: Box<dyn BufRead> = if args.len() >= 2 {
        // 我传了文件名：比如 cargo run -- /tmp/trace.log
        let path = &args[1];
        Box::new(BufReader::new(File::open(path)?))
    } else {
        // 我没传文件名：那就从 stdin 读
        Box::new(BufReader::new(io::stdin()))
    };
    // BTreeMap 的好处：输出时会按 key 排序，看着更整齐。
    // key: opcode 名字（比如 PUSHGLOBAL）
    // value: 计数次数
    let mut counts: BTreeMap<String, u64> = BTreeMap::new();

    // total 用来统计我一共读到了多少条 trace 指令
    let mut total: u64 = 0;

    // 开始逐行读输入
    for line in reader.lines() {
        let line = line?;

        // 我们只关心 trace 行，非 trace 行直接跳过
        // trace 行长这样：
        // [TRACE] pc=0x...  op=19(PUSHGLOBAL)  stack=1
        if !line.contains("[TRACE]") {
            continue;
        }

        // 我现在要从这行里把 "PUSHGLOBAL" 这种名字抠出来
        //
        // 关键点：
        // - 先找 "op=" 的位置
        // - 然后找 '(' 和 ')' 中间的内容
        //
        // 例如：
        //   op=19(PUSHGLOBAL)
        //             ^^^^^^^ 这段就是我要的

        if let Some(pos) = line.find("op=") {
            // tail 从 "op=" 后面开始，比如：
            // 19(PUSHGLOBAL)  stack=1
            let tail = &line[pos + 3..];

            // 找左括号 '('
            if let Some(lparen) = tail.find('(') {
                // 在 '(' 后面继续找右括号 ')'
                // 注意：这里 find 返回的是相对下标，所以我用 tail[lparen+1..] 再找一次
                if let Some(rparen_rel) = tail[lparen + 1..].find(')') {
                    // 把名字切出来：
                    // name = '(' 和 ')' 中间那段
                    let name = &tail[lparen + 1..lparen + 1 + rparen_rel];

                    // 放进 map 里计数 +1
                    *counts.entry(name.to_string()).or_insert(0) += 1;
                    total += 1;
                }
            }
        }
    }

    // 输出统计结果（给我自己看的，所以我写得很直白）
    println!("=== Lua Trace Opcode 统计 ===");
    println!("总 trace 指令数：{}", total);
    println!();

    // 逐项打印：名字 + 次数
    // BTreeMap 会自动按名字排序，所以输出看起来会比较整齐
    for (name, c) in &counts {
        println!("{:<15} {}", name, c);
    }

    Ok(())
}
