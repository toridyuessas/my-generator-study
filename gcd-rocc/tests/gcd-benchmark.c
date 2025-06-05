#include "rocc.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

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

// ソフトウェア参照実装（引き算ベース - ハードウェアと同じアルゴリズム）
unsigned long gcd_ref(unsigned long a, unsigned long b) {
    unsigned long cycles = 0;
    while (b != 0) {
        cycles++;
        if (a > b) {
            a = a - b;
        } else {
            b = b - a;
        }
    }
    return a;
}

// 高速ソフトウェア実装（剰余ベース）
unsigned long gcd_fast(unsigned long a, unsigned long b) {
    while (b != 0) {
        unsigned long temp = a % b;
        a = b;
        b = temp;
    }
    return a;
}

// テストデータセット
typedef struct {
    unsigned long a, b;
    const char* description;
} test_case_t;

static test_case_t test_cases[] = {
    {48, 18, "小さな数値"},
    {1071, 462, "中程度の数値"},
    {12345, 6789, "一般的なサイズ"},
    {999999, 123456, "大きな数値"},
    {1000000007, 999999937, "大きな素数ペア"},
    {987654321, 123456789, "最大規模"},
    {1000000000, 1, "最悪ケース1"},
    {999999999, 999999998, "最悪ケース2"}
};

#define NUM_TEST_CASES (sizeof(test_cases)/sizeof(test_cases[0]))

// 実行時間測定用マクロ（rdcycle命令使用）
static inline unsigned long read_cycles() {
    unsigned long cycles;
    asm volatile ("rdcycle %0" : "=r" (cycles));
    return cycles;
}

void run_benchmark() {
    printf("===== GCD アクセラレータ性能ベンチマーク =====\n\n");
    printf("テストケース数: %d\n", NUM_TEST_CASES);
    printf("測定方法: RISC-V rdcycle命令によるサイクル数測定\n\n");
    
    printf("%-15s %-12s %-12s | %-8s %-8s %-8s | %-6s %-6s\n", 
           "数値ペア", "a", "b", "HW", "SW同じ", "SW高速", "HW/SW", "HW/高速");
    printf("----------------------------------------------------------------------------\n");
    
    unsigned long total_hw_cycles = 0;
    unsigned long total_sw_cycles = 0;
    unsigned long total_fast_cycles = 0;
    
    for (int i = 0; i < NUM_TEST_CASES; i++) {
        unsigned long a = test_cases[i].a;
        unsigned long b = test_cases[i].b;
        
        // === ハードウェア実装の測定 ===
        unsigned long hw_start = read_cycles();
        gcd_start(a, b);
        unsigned long hw_result = gcd_read();
        unsigned long hw_end = read_cycles();
        unsigned long hw_cycles = hw_end - hw_start;
        
        // === ソフトウェア実装（同じアルゴリズム）の測定 ===
        unsigned long sw_start = read_cycles();
        unsigned long sw_result = gcd_ref(a, b);
        unsigned long sw_end = read_cycles();
        unsigned long sw_cycles = sw_end - sw_start;
        
        // === 高速ソフトウェア実装の測定 ===
        unsigned long fast_start = read_cycles();
        unsigned long fast_result = gcd_fast(a, b);
        unsigned long fast_end = read_cycles();
        unsigned long fast_cycles = fast_end - fast_start;
        
        // 結果検証
        if (hw_result != sw_result || sw_result != fast_result) {
            printf("ERROR: 結果不一致! HW=%lu, SW=%lu, Fast=%lu\n", 
                   hw_result, sw_result, fast_result);
            continue;
        }
        
        // スピードアップ比計算
        double hw_vs_sw = (double)sw_cycles / hw_cycles;
        double hw_vs_fast = (double)fast_cycles / hw_cycles;
        
        printf("%-15s %-12lu %-12lu | %-8lu %-8lu %-8lu | %-6.2fx %-6.2fx\n",
               test_cases[i].description, a, b, 
               hw_cycles, sw_cycles, fast_cycles,
               hw_vs_sw, hw_vs_fast);
        
        total_hw_cycles += hw_cycles;
        total_sw_cycles += sw_cycles;
        total_fast_cycles += fast_cycles;
    }
    
    printf("----------------------------------------------------------------------------\n");
    printf("合計サイクル数: HW=%lu, SW=%lu, Fast=%lu\n", 
           total_hw_cycles, total_sw_cycles, total_fast_cycles);
    printf("平均スピードアップ: HW vs SW同じ = %.2fx, HW vs SW高速 = %.2fx\n",
           (double)total_sw_cycles / total_hw_cycles,
           (double)total_fast_cycles / total_hw_cycles);
}

void detailed_analysis() {
    printf("\n===== 詳細分析: 数値サイズ vs 性能 =====\n");
    
    // 様々なサイズの数値ペアで性能特性を調査
    unsigned long sizes[] = {100, 1000, 10000, 100000, 1000000, 10000000};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    printf("%-10s %-10s %-10s %-10s %-8s\n", "数値サイズ", "HW cycles", "SW cycles", "Fast cycles", "HW/SW");
    printf("--------------------------------------------------------\n");
    
    for (int i = 0; i < num_sizes; i++) {
        unsigned long size = sizes[i];
        unsigned long a = size + 123;  // 適度に大きな値
        unsigned long b = size - 456;  // 異なる値
        if (b <= 0) b = size / 2;
        
        // 測定
        unsigned long hw_start = read_cycles();
        gcd_start(a, b);
        unsigned long hw_result = gcd_read();
        unsigned long hw_cycles = read_cycles() - hw_start;
        
        unsigned long sw_start = read_cycles();
        unsigned long sw_result = gcd_ref(a, b);
        unsigned long sw_cycles = read_cycles() - sw_start;
        
        unsigned long fast_start = read_cycles();
        unsigned long fast_result = gcd_fast(a, b);
        unsigned long fast_cycles = read_cycles() - fast_start;
        
        if (hw_result == sw_result && sw_result == fast_result) {
            printf("%-10lu %-10lu %-10lu %-10lu %-8.2fx\n",
                   size, hw_cycles, sw_cycles, fast_cycles, 
                   (double)sw_cycles / hw_cycles);
        } else {
            printf("%-10lu ERROR: 結果不一致\n", size);
        }
    }
}

int main(void)
{
    printf("GCD アクセラレータ ベンチマークテスト開始\n");
    printf("測定対象: ハードウェア vs ソフトウェア実装\n\n");
    
    run_benchmark();
    detailed_analysis();
    
    printf("\n===== ベンチマーク完了 =====\n");
    printf("重要な観察ポイント:\n");
    printf("1. ハードウェアは固定サイクル数で実行されるか？\n");
    printf("2. 大きな数値でソフトウェアの実行時間は増加するか？\n");
    printf("3. 最適化されたソフトウェア（剰余ベース）との比較\n");
    printf("4. どのようなケースでハードウェア化が有効か？\n");
    
    return 0;
}