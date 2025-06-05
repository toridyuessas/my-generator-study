# GCD RoCC アクセラレータ チュートリアル

Chipyardで独自のRoCCアクセラレータを作成する完全ガイド

## 目次

1. [概要](#概要)
2. [前提知識](#前提知識)
3. [環境セットアップ](#環境セットアップ)
4. [Chipyardの構造理解](#chipyardの構造理解)
5. [GCD RoCCアクセラレータの実装](#gcd-roccアクセラレータの実装)
6. [テストプログラムの作成](#テストプログラムの作成)
7. [ビルドとシミュレーション](#ビルドとシミュレーション)
8. [トラブルシューティング](#トラブルシューティング)
9. [学習のポイント](#学習のポイント)
10. [次のステップ](#次のステップ)

---

## 概要

### このチュートリアルの目標
- ChipyardでRoCCアクセラレータの基本を学ぶ
- 最小構成のGCDアクセラレータを実装する
- Chisel HDLとRocket Chipの理解を深める
- ハードウェア・ソフトウェア協調設計の基礎を習得する

### なぜGCDアクセラレータ？
- **アルゴリズムが単純**: ユークリッド互除法（引き算ベース）
- **ハードウェア向き**: 複雑な演算が不要
- **学習効果が高い**: RoCCインターフェースの基本を学べる
- **デバッグしやすい**: 結果が予測可能

---

## 前提知識

### 必要な技術背景
- **基本的なVerilog/Chisel知識**
- **RISC-V ISAの理解**
- **Linux/Unix環境での開発経験**
- **Git操作の基本**

### RoCCとは
**RoCC (Rocket Custom Coprocessor)** は、Rocket CPUに密結合されたカスタムアクセラレータインターフェース。

#### RoCCの特徴
```
CPU ←→ RoCC アクセラレータ
     ↑ カスタム命令
     ↓ 結果
```

- カスタム命令経由でアクセス
- CPUレジスタとの直接やり取り
- メモリアクセス可能
- 低レイテンシ通信

---

## 環境セットアップ

### 1. Chipyard環境の準備

```bash
# Chipyardクローン
git clone https://github.com/ucb-bar/chipyard.git
cd chipyard

# 環境構築（時間がかかります）
./build-setup.sh riscv-tools

# 環境変数設定
source env.sh
```

### 2. 環境確認

```bash
# RISC-V toolchainの確認
which riscv64-unknown-elf-gcc
riscv64-unknown-elf-gcc --version

# Condaの確認
conda env list
```

**期待される出力:**
```
riscv64-unknown-elf-gcc (g2ee5e430018) 12.2.0
```

---

## Chipyardの構造理解

### ディレクトリ構造
```
chipyard/
├── generators/          # ハードウェアジェネレータ
│   ├── boom/           # BOOM CPU
│   ├── rocket-chip/    # Rocket CPU
│   ├── gemmini/        # 行列演算アクセラレータ
│   └── chipyard/       # Chipyard統合
├── sims/               # シミュレーション環境
│   ├── verilator/      # Verilatorシミュレータ
│   └── vcs/           # VCSシミュレータ
├── software/           # ソフトウェア開発環境
├── tests/             # テストプログラム
└── tools/             # 開発ツール
```

### 重要なファイル
- **`build.sbt`**: プロジェクト依存関係
- **`env.sh`**: 環境変数設定
- **Config files**: ハードウェア設定

---

## GCD RoCCアクセラレータの実装

### 1. プロジェクトディレクトリ作成

```bash
mkdir -p generators/gcd-rocc/src/main/scala
```

### 2. build.sbtの作成

**`generators/gcd-rocc/build.sbt`**
```scala
name := "gcd-rocc"

organization := "edu.berkeley.cs"

version := "1.0-SNAPSHOT"

scalaVersion := "2.13.10"

resolvers ++= Seq(
  Resolver.sonatypeRepo("snapshots"),
  Resolver.sonatypeRepo("releases"),
  Resolver.mavenLocal
)

val chiselVersion = "3.6.0"

libraryDependencies ++= Seq(
  "edu.berkeley.cs" %% "chisel3" % chiselVersion,
  "edu.berkeley.cs" %% "rocketchip" % "1.6.0"
)

addCompilerPlugin("edu.berkeley.cs" % "chisel3-plugin" % chiselVersion cross CrossVersion.full)
```

### 3. メインアクセラレータの実装

**`generators/gcd-rocc/src/main/scala/GCDRoCC.scala`**
```scala
package gcdrocc

import chisel3._
import chisel3.util._
import org.chipsalliance.cde.config._
import freechips.rocketchip.subsystem._
import freechips.rocketchip.diplomacy._
import freechips.rocketchip.rocket._
import freechips.rocketchip.tilelink._
import freechips.rocketchip.tile._

// カスタム命令定義
object GCDInstr {
  val START = 0.U  // GCD計算開始
  val READ  = 1.U  // 結果読み出し
}

class GCDRoCC(opcodes: OpcodeSet)(implicit p: Parameters) extends LazyRoCC(opcodes) {
  override lazy val module = new GCDRoCCModuleImp(this)
}

class GCDRoCCModuleImp(outer: GCDRoCC)(implicit p: Parameters) extends LazyRoCCModuleImp(outer) {
  
  // 状態機械の定義
  val s_idle :: s_compute :: s_done :: Nil = Enum(3)
  val state = RegInit(s_idle)
  
  // GCD計算用レジスタ
  val a = Reg(UInt(64.W))
  val b = Reg(UInt(64.W))
  val result = Reg(UInt(64.W))
  
  // コマンドキュー
  val cmd = Queue(io.cmd)
  
  // デフォルト出力
  io.resp.valid := false.B
  io.resp.bits := DontCare
  io.busy := state =/= s_idle
  io.interrupt := false.B
  cmd.ready := false.B
  
  // メイン状態機械
  switch(state) {
    is(s_idle) {
      when(cmd.valid) {
        switch(cmd.bits.inst.funct) {
          is(GCDInstr.START) {
            // GCD計算開始
            a := cmd.bits.rs1
            b := cmd.bits.rs2
            state := s_compute
            cmd.ready := true.B
            printf("GCD RoCC: Starting computation with a=%d, b=%d\n", cmd.bits.rs1, cmd.bits.rs2)
          }
          is(GCDInstr.READ) {
            // 結果返却
            io.resp.valid := true.B
            io.resp.bits.rd := cmd.bits.inst.rd
            io.resp.bits.data := result
            cmd.ready := true.B
            printf("GCD RoCC: Reading result=%d\n", result)
          }
        }
      }
    }
    
    is(s_compute) {
      // ユークリッド互除法（引き算ベース）
      when(b === 0.U) {
        result := a
        state := s_done
        printf("GCD RoCC: Computation complete, result=%d\n", a)
      } .elsewhen(a > b) {
        a := a - b
      } .otherwise {
        b := b - a
      }
    }
    
    is(s_done) {
      // 次のコマンド待ち
      state := s_idle
    }
  }
  
  // メモリインターフェース（今回は未使用）
  io.mem.req.valid := false.B
  io.mem.req.bits := DontCare
}

// Configuration fragment
class WithGCDRoCC extends Config((site, here, up) => {
  case BuildRoCC => up(BuildRoCC) ++ Seq(
    (p: Parameters) => {
      val gcd = LazyModule(new GCDRoCC(OpcodeSet.custom0)(p))
      gcd
    })
})

// テスト用設定
class GCDRoCCRocketConfig extends Config(
  new WithGCDRoCC ++
  new freechips.rocketchip.subsystem.WithNBigCores(1) ++
  new chipyard.config.AbstractConfig
)
```

### 4. ビルドシステム統合

**`build.sbt`に追加:**
```scala
lazy val gcd_rocc = (project in file("generators/gcd-rocc"))
  .dependsOn(rocketchip)
  .settings(libraryDependencies ++= rocketLibDeps.value)
  .settings(commonSettings)

// chipyardプロジェクトの依存関係に追加
lazy val chipyard = (project in file("generators/chipyard"))
  .dependsOn(testchipip, rocketchip, boom, hwacha, rocketchip_blocks, rocketchip_inclusive_cache, iocell,
    sha3, dsptools, rocket_dsp_utils,
    gemmini, icenet, tracegen, cva6, nvdla, sodor, ibex, fft_generator,
    constellation, mempress, barf, shuttle, caliptra_aes, gcd_rocc)
```

**`generators/chipyard/src/main/scala/config/RoCCAcceleratorConfigs.scala`に追加:**
```scala
class GCDRoCCRocketConfig extends Config(
  new gcdrocc.WithGCDRoCC ++                                      // use GCD RoCC accelerator
  new freechips.rocketchip.subsystem.WithNBigCores(1) ++
  new chipyard.config.AbstractConfig)
```

---

## テストプログラムの作成

### 1. テストプログラム実装

**`tests/gcd-rocc.c`**
```c
#include "rocc.h"
#include <stdio.h>

// GCD RoCC instruction macros
#define GCD_START 0
#define GCD_READ  1

// ヘルパー関数
static inline void gcd_start(unsigned long a, unsigned long b)
{
    ROCC_INSTRUCTION_SS(0, a, b, GCD_START);
}

static inline unsigned long gcd_read()
{
    unsigned long result;
    ROCC_INSTRUCTION_D(0, result, GCD_READ);
    return result;
}

// ソフトウェア参照実装
unsigned long gcd_ref(unsigned long a, unsigned long b) {
    while (b != 0) {
        if (a > b) {
            a = a - b;
        } else {
            b = b - a;
        }
    }
    return a;
}

int main(void)
{
    // テストケース1: 基本的なGCD
    unsigned long a1 = 48, b1 = 18;
    unsigned long expected1 = gcd_ref(a1, b1);
    
    printf("Testing GCD RoCC: gcd(%lu, %lu)\n", a1, b1);
    printf("Expected result: %lu\n", expected1);
    
    gcd_start(a1, b1);
    unsigned long result1 = gcd_read();
    printf("Hardware result: %lu\n", result1);
    
    if (result1 != expected1) {
        printf("FAIL: Test 1 failed - expected %lu, got %lu\n", expected1, result1);
        return 1;
    }
    printf("PASS: Test 1 passed\n\n");
    
    // 追加のテストケース...
    
    printf("All GCD RoCC tests passed!\n");
    return 0;
}
```

### 2. Makefileの更新

**`tests/Makefile`に追加:**
```makefile
PROGRAMS = pwm blkdev accum charcount nic-loopback big-blkdev pingd \
           streaming-passthrough streaming-fir nvdla spiflashread spiflashwrite fft gcd \
           hello mt-hello symmetric gcd-rocc
```

### 3. テストプログラムのビルド

```bash
source env.sh
cd tests
riscv64-unknown-elf-gcc -std=gnu99 -O2 -fno-common -fno-builtin-printf -Wall \
  -march=rv64imafdc -mabi=lp64d -specs=htif_nano.specs -static \
  gcd-rocc.c -o gcd-rocc
```

---

## ビルドとシミュレーション

### 1. RTL生成

```bash
source env.sh
cd sims/verilator
make CONFIG=GCDRoCCRocketConfig
```

**注意**: 初回ビルドは非常に時間がかかります（30分〜数時間）

### 2. シミュレーション実行

```bash
make CONFIG=GCDRoCCRocketConfig run-binary BINARY=../../tests/gcd-rocc
```

### 3. 期待される出力

```
Testing GCD RoCC: gcd(48, 18)
Expected result: 6
GCD RoCC: Starting computation with a=48, b=18
GCD RoCC: Computation complete, result=6
GCD RoCC: Reading result=6
Hardware result: 6
PASS: Test 1 passed

All GCD RoCC tests passed!
```

---

## トラブルシューティング

### よくある問題と解決法

#### 1. Chiselバージョンエラー
```
Error downloading org.chipsalliance:chisel-stdlib_2.13:5.1.0
```
**原因**: ChipyardはChisel 3.6.0を使用しているが、5.1.0を指定した  
**解決法**: 
```scala
// ❌ 間違い
val chiselVersion = "5.1.0"
"org.chipsalliance" %% "chisel" % chiselVersion

// ✅ 正しい  
val chiselVersion = "3.6.0"
"edu.berkeley.cs" %% "chisel3" % chiselVersion
```

#### 2. パッケージインポートエラー
```
Error: package gcdrocc not found
```
**解決法**: `build.sbt`の依存関係を確認

#### 2. 環境変数エラー
```
RISCV toolchain not found
```
**解決法**: `source env.sh`を実行

#### 3. 循環依存エラー
```
Error: package chipyard.config not found
```
**原因**: GCD RoCCプロジェクトがchipyardプロジェクトを参照している  
**解決法**: 設定クラスを適切な場所に配置
```scala
// ❌ 間違い - GCDRoCC.scala内で設定クラスを定義
class GCDRoCCRocketConfig extends Config(...)

// ✅ 正解 - chipyardプロジェクト内のRoCCAcceleratorConfigs.scalaで定義
// generators/chipyard/src/main/scala/config/RoCCAcceleratorConfigs.scala
class WithGCDRoCC extends Config(...)
class GCDRoCCRocketConfig extends Config(...)
```

#### 4. Condaエラー
```
CondaError: Run 'conda init' before 'conda activate'
```
**解決法**: 
```bash
source /home/user/miniconda3/etc/profile.d/conda.sh
source env.sh
```

#### 4. コンパイルエラー
```
cc: error: unrecognized argument in option '-mabi=lp64d'
```
**解決法**: システムのccではなく、RISC-V gccを使用

### デバッグのヒント

1. **printf文を活用**: Chiselコードでprintfを使ってデバッグ
2. **波形観察**: VCDファイルを生成して波形を確認
3. **段階的テスト**: 簡単なテストから始めて複雑化

---

## 学習のポイント

### 技術的な学習

#### 1. RoCCインターフェースの理解
- **LazyRoCC**: DiplomacyベースのRoCCベースクラス
- **カスタム命令**: CUSTOM_0〜CUSTOM_3オペコード
- **コマンド・レスポンス**: `cmd`と`resp`インターフェース

#### 2. Chisel HDLの特徴
- **型安全**: コンパイル時のエラー検出
- **ジェネリック**: パラメータ化されたハードウェア
- **関数型**: 関数型プログラミングの利点

#### 3. Chipyardの設計思想
- **モジュラー設計**: コンポーネントの組み合わせ
- **Configuration**: 型安全な設定システム
- **Diplomacy**: ポートの自動接続

### 設計上の学習

#### 1. ハードウェア・ソフトウェア協調設計
- **インターフェース設計**: ハードウェアとソフトウェアの境界
- **性能最適化**: ハードウェアアクセラレーションの効果
- **検証戦略**: テストプログラムとリファレンス実装

#### 2. SoC設計の考え方
- **システム統合**: 各コンポーネントの役割分担
- **通信プロトコル**: TileLink, AXI4などの理解
- **メモリ階層**: キャッシュとメモリシステム

---

## 次のステップ

### 1. パフォーマンス分析 📊 (推奨: 最優先)

#### ベンチマーク実装
GCDアクセラレータの実際の性能を測定して、ハードウェア化の効果を定量的に確認します。

```c
// tests/gcd-benchmark.c (新規作成推奨)
#include "rocc.h"
#include <stdio.h>
#include <time.h>

// ベンチマーク用データセット
static unsigned long test_pairs[][2] = {
    {48, 18}, {1071, 462}, {3024, 1512}, {12345, 6789},
    // より大きな数値でのテスト
    {999999999, 888888888}, {1234567890, 987654321}
};

int main() {
    printf("=== GCD アクセラレータ性能ベンチマーク ===\n");
    
    for(int i = 0; i < sizeof(test_pairs)/sizeof(test_pairs[0]); i++) {
        unsigned long a = test_pairs[i][0], b = test_pairs[i][1];
        
        // ハードウェア実装の測定
        clock_t hw_start = clock();
        gcd_start(a, b);
        unsigned long hw_result = gcd_read();
        clock_t hw_time = clock() - hw_start;
        
        // ソフトウェア実装の測定
        clock_t sw_start = clock();
        unsigned long sw_result = gcd_ref(a, b);
        clock_t sw_time = clock() - sw_start;
        
        printf("gcd(%lu, %lu): HW=%lu (%ld cycles), SW=%lu (%ld cycles), Speedup=%.2fx\n",
               a, b, hw_result, hw_time, sw_result, sw_time, 
               (double)sw_time / hw_time);
    }
    return 0;
}
```

#### 期待される学習効果
- ハードウェアアクセラレーションの具体的効果
- 大きな数値での性能特性の理解
- クロックサイクル単位での詳細分析

### 2. アーキテクチャ拡張 🏗️

#### 2.1 メモリアクセス機能付きGCD
```scala
// より高度な実装例
class GCDWithMemory(opcodes: OpcodeSet)(implicit p: Parameters) extends LazyRoCC(opcodes) {
  override lazy val module = new GCDWithMemoryModuleImp(this)
}

class GCDWithMemoryModuleImp(outer: GCDWithMemory) extends LazyRoCCModuleImp(outer) {
  // メモリから数値ペアを読み込んで連続的にGCD計算
  // 結果をメモリに書き戻し
}
```

#### 2.2 並列GCD計算器
```scala
// 複数のGCDを同時計算
class ParallelGCD(numUnits: Int, opcodes: OpcodeSet) extends LazyRoCC(opcodes) {
  val gcdUnits = Seq.fill(numUnits)(Module(new GCDUnit))
  // ラウンドロビンスケジューリング
}
```

### 3. より高度なアクセラレータ 🚀

#### 推奨順序
1. **FFTアクセラレータ** (次回推奨)
   - 信号処理の基本
   - 複素数演算の学習
   - パイプライン設計の実践

2. **行列乗算アクセラレータ**
   - AI/ML分野への応用
   - データフロー設計
   - メモリ階層の最適化

3. **暗号化エンジン**
   - AES、RSA実装
   - セキュリティ考慮事項
   - 高スループット設計

#### FFTアクセラレータの基本構造
```scala
class FFTRoCC(opcodes: OpcodeSet)(implicit p: Parameters) extends LazyRoCC(opcodes) {
  // バタフライ演算ユニット
  // 回転因子テーブル
  // ビットリバース機能
}
```

### 4. 産業レベル設計への展開 🌉

#### 4.1 FPGA実装
```bash
# 実際のハードウェアでの動作確認
cd fpga
make CONFIG=GCDRoCCRocketConfig fpga-bitstream

# FPGAボードでのテスト実行
make CONFIG=GCDRoCCRocketConfig fpga-run BINARY=../../tests/gcd-rocc
```

#### 4.2 VLSI設計フロー
```bash
# Sky130 PDKを使った物理設計
cd vlsi
make CONFIG=GCDRoCCRocketConfig tech=sky130 VLSI_TOP=ChipTop

# 設計ルールチェック
make CONFIG=GCDRoCCRocketConfig tech=sky130 drc

# レイアウト vs 回路図チェック
make CONFIG=GCDRoCCRocketConfig tech=sky130 lvs
```

### 5. デバッグ技術の深掘り 🔧

#### 波形解析の活用
```bash
# 詳細な波形生成
make CONFIG=GCDRoCCRocketConfig debug BINARY=../../tests/gcd-rocc

# GTKWaveで波形確認
gtkwave generated-src/chipyard.TestHarness.GCDRoCCRocketConfig/chipyard.TestHarness.GCDRoCCRocketConfig.vcd
```

#### 高度なデバッグ手法
```scala
// ChiselのAssertを活用
assert(state =/= s_idle || !io.busy, "Busy signal should be false in idle state")

// カバレッジ追加
cover(state === s_compute && a === b, "GCD_EQUAL_VALUES")
```

### 6. 開発手順の標準化 📋

#### アクセラレータ開発テンプレート
```
generators/your-accelerator/
├── build.sbt                    # 標準ビルド設定
├── src/main/scala/
│   ├── YourAccelerator.scala   # メイン実装
│   └── YourAcceleratorUtils.scala  # ヘルパー関数
├── tests/
│   ├── your-accelerator.c      # C言語テスト
│   └── benchmark.c             # 性能測定
└── docs/
    └── README.md               # 実装ドキュメント
```

#### 開発チェックリスト
- [ ] 基本機能の実装
- [ ] テストプログラムの作成
- [ ] ソフトウェア参照実装との比較
- [ ] 性能ベンチマークの実施
- [ ] エラーハンドリングの実装
- [ ] ドキュメント作成

### 学習リソース 📚

#### 必読ドキュメント
- [Rocket Chip Generator](https://github.com/freechipsproject/rocket-chip)
- [Chisel Tutorial](https://github.com/freechipsproject/chisel-tutorial)
- [TileLink Specification](https://starfive-tech.github.io/freedom/doc/tilelink/)

#### コミュニティリソース
- [Chipyard Documentation](https://chipyard.readthedocs.io/)
- [RISC-V Foundation](https://riscv.org/)
- [OpenROAD Project](https://theopenroadproject.org/)

---

## 付録

### A. 重要なファイル一覧

```
generators/gcd-rocc/
├── build.sbt                    # ビルド設定
└── src/main/scala/GCDRoCC.scala # メイン実装

tests/
├── gcd-rocc.c                   # テストプログラム
└── Makefile                     # ビルド設定

docs/
└── gcd-rocc-tutorial.md         # このドキュメント
```

### B. 参考リンク

- [Chipyard Documentation](https://chipyard.readthedocs.io/)
- [Rocket Chip Generator](https://github.com/freechipsproject/rocket-chip)
- [Chisel Language Reference](https://www.chisel-lang.org/)
- [RISC-V ISA Manual](https://riscv.org/specifications/)

### C. 用語集

| 用語 | 説明 |
|------|------|
| **RoCC** | Rocket Custom Coprocessor |
| **LazyRoCC** | DiplomacyベースのRoCCベースクラス |
| **TileLink** | Chip間通信プロトコル |
| **Diplomacy** | ポート自動接続フレームワーク |
| **Chisel** | ハードウェア記述言語 |
| **FIRRTL** | Chiselの中間表現 |

---

**作成日**: 2025年6月5日  
**更新日**: 2025年6月5日  
**作成者**: Claude Code Assistant  
**バージョン**: 1.0

このチュートリアルが、Chipyardを使ったカスタムアクセラレータ開発の第一歩となることを願っています。