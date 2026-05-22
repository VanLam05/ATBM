# Influence Minimization via Node Blocking

Repo chứa các baseline:

| Thuật toán          | Code                                 |
| ------------------- | ------------------------------------ |
| AdvancedGreedy (AG) | `AdvancedGreedy/advanced_greedy.cpp` |
| GreedyReplace (GR)  | `AdvancedGreedy/advanced_greedy.cpp` |
| IMin / SandIMIN     | `IMin/SandIMIN_code/`                |

Tất cả thuật toán dùng chung dataset trong folder `datasets/` và nên ghi output về folder `results/` ở root project.

## Dataset

Format dataset chung:

```text
n m
directed|undirected
u1 v1
u2 v2
...
```

Ví dụ:

```text
datasets/p2p-Gnutella31.txt
```

Mô hình xác suất cạnh là Weighted Cascade:

```text
p(u, v) = 1 / in_degree(v)
```

## Build

### macOS

Chạy từ root project:

```bash
cd code/AdvancedGreedy
g++ -O3 -std=c++17 -o ag advanced_greedy.cpp
g++ -O3 -std=c++17 -o experiment experiment.cpp

cd ../IMin
g++ -O3 -std=c++17 -o prepare_dataset prepare_dataset.cpp

cd SandIMIN_code
g++ -O3 -std=c++17 -o IMIN Sandwich.cpp sfmt/SFMT.c
```

### Windows MSYS2 MinGW-w64

```bash
cd code/AdvancedGreedy
g++ -O3 -std=c++17 -o ag.exe advanced_greedy.cpp
g++ -O3 -std=c++17 -o experiment.exe experiment.cpp

cd ../IMin
g++ -O3 -std=c++17 -o prepare_dataset.exe prepare_dataset.cpp

cd SandIMIN_code
g++ -O3 -std=c++17 -o IMIN.exe Sandwich.cpp sfmt/SFMT.c -lpsapi
```

## Seed Cố Định

Để các thuật toán so sánh công bằng, dùng cùng một seed file. File seed có format mỗi node một dòng:

```text
850
2450
12145
...
```

Ví dụ seed file:

```text
results/p2p-Gnutella31_seed_node_10.txt
```

Nếu `-seedFile` chưa tồn tại, AdvancedGreedy và IMin sẽ tự sinh seed từ top-200 node theo out-degree rồi ghi vào file đó.

## Chạy AdvancedGreedy / GreedyReplace

### macOS

```bash
mkdir -p results

cd code/AdvancedGreedy
./ag -dataset ../../datasets/p2p-Gnutella31.txt \
  -k 100 \
  -seedNum 50 \
  -theta 100 \
  -mc 10000 \
  -algo AG \
  -seedFile ../../datasets/p2p-Gnutella31_seed_node_10.txt \
  -output ../../results/ag_results.tsv
```

Đổi `-algo AG` thành `-algo GR` hoặc `-algo BOTH` nếu muốn chạy GreedyReplace hoặc cả hai.

### Windows PowerShell

```powershell
New-Item -ItemType Directory -Force results

cd code\AdvancedGreedy
.\ag.exe -dataset ..\..\datasets\p2p-Gnutella31.txt `
  -k 100 `
  -seedNum 10 `
  -theta 100 `
  -mc 10000 `
  -algo AG `
  -seedFile ..\..\datasets\p2p-Gnutella31_seed_node_10.txt `
  -output ..\..\results\ag_results.tsv
```

## Chạy IMin / SandIMIN

IMin hiện đọc trực tiếp dataset chung trong `datasets/`, không cần copy sang `IMin/SandIMIN_code/dataset/`.

### macOS

```bash
mkdir -p results

cd code/IMin/SandIMIN_code
./IMIN -dataset ../../../datasets/email-EuAll.txt \
  -k 100 \
  -rumorNum 10 \
  -algo SandIMIN \
  -epsilon 0.2 \
  -gamma 0.1 \
  -beta 0.1 \
  -seedFile ../../../datasets/email-EuAll_seed_10.txt \
  -outputDir ../../../results
```

Với lệnh trên, output được ghi vào:

```text
results/res_p2p-Gnutella31_|S|=10_K=100_epsilon=0.200000_gamma=0.100000_beta=0.100000_algo=SandIMIN
```

### Windows PowerShell

```powershell
New-Item -ItemType Directory -Force results

cd code\IMin\SandIMIN_code
.\IMIN.exe -dataset ..\..\..\datasets\p2p-Gnutella31.txt `
  -k 100 `
  -rumorNum 10 `
  -algo SandIMIN `
  -epsilon 0.2 `
  -gamma 0.1 `
  -beta 0.1 `
  -seedFile ..\..\..\datasets\p2p-Gnutella31_seed_node_10.txt `
  -outputDir ..\..\..\results
```

## Format Output IMin

IMin ghi TSV với các cột:

```text
algorithm	dataset	k	|S|	result_type	spread_before	spread_after	saved_nodes	time_seconds	approx_ratio	status
```

Các `result_type` chính:

| result_type | Ý nghĩa                           |
| ----------- | --------------------------------- |
| `BEST`      | Kết quả SandIMIN chọn là tốt nhất |
| `LB`        | Lower-bound candidate             |
| `UB`        | Upper-bound candidate             |
| `OR`        | Degree-based heuristic            |

Các cột dùng để điền bảng:

| Cột            | Dùng cho bảng                      |
| -------------- | ---------------------------------- |
| `saved_nodes`  | Số nút cứu được                    |
| `time_seconds` | Thời gian chạy                     |
| `approx_ratio` | Tỷ lệ xấp xỉ nếu thuật toán có ghi |

## Chạy Toàn Bộ Experiment

Script này build code, chạy AG/GR, chạy IMin, và sinh bảng CSV trong `results/`.

### macOS / Linux

```bash
cd code
bash run_experiments.sh small
```

Chạy full:

```bash
cd code
bash run_experiments.sh full
```

`small` dùng tham số nhỏ để test nhanh. `full` chạy theo kịch bản:

- `k = 100, 200, 300, 400, 500`, cố định `|S| = 10`
- `|S| = 10, 20, 30, 40, 50`, cố định `k = 100`

## Sinh Bảng Kết Quả

Nếu đã có raw output trong `results/`, chạy:

```bash
cd code
python3 summarize_results.py --results ../results
```

Script sinh các file:

| File                             | Nội dung                            |
| -------------------------------- | ----------------------------------- | --- | ------------------- |
| `results/table_k_saved.csv`      | Số nút cứu được khi `k` thay đổi, ` | S   | = 10`               |
| `results/table_k_time.csv`       | Thời gian chạy khi `k` thay đổi, `  | S   | = 10`               |
| `results/table_s_saved.csv`      | Số nút cứu được khi `               | S   | `thay đổi,`k = 100` |
| `results/table_s_time.csv`       | Thời gian chạy khi `                | S   | `thay đổi,`k = 100` |
| `results/table_approx_ratio.csv` | Tỷ lệ xấp xỉ nếu có                 |

## Smoke Test Nhanh

### macOS

```bash
mkdir -p results

cd code/AdvancedGreedy
./ag -dataset ../../datasets/p2p-Gnutella31.txt \
  -k 1 \
  -seedNum 10 \
  -theta 2 \
  -mc 20 \
  -algo AG \
  -seedFile ../../results/p2p-Gnutella31_seed_node_10.txt \
  -output ../../results/ag_smoke.tsv

cd ../IMin/SandIMIN_code
./IMIN -dataset ../../../datasets/p2p-Gnutella31.txt \
  -k 1 \
  -rumorNum 10 \
  -algo SandIMIN \
  -epsilon 0.2 \
  -gamma 0.1 \
  -beta 0.1 \
  -seedFile ../../../results/p2p-Gnutella31_seed_node_10.txt \
  -outputDir ../../../results
```

### Windows PowerShell

```powershell
New-Item -ItemType Directory -Force results

cd code\AdvancedGreedy
.\ag.exe -dataset ..\..\datasets\p2p-Gnutella31.txt `
  -k 1 `
  -seedNum 10 `
  -theta 2 `
  -mc 20 `
  -algo AG `
  -seedFile ..\..\results\p2p-Gnutella31_seed_node_10.txt `
  -output ..\..\results\ag_smoke.tsv

cd ..\IMin\SandIMIN_code
.\IMIN.exe -dataset ..\..\..\datasets\p2p-Gnutella31.txt `
  -k 1 `
  -rumorNum 10 `
  -algo SandIMIN `
  -epsilon 0.2 `
  -gamma 0.1 `
  -beta 0.1 `
  -seedFile ..\..\..\results\p2p-Gnutella31_seed_node_10.txt `
  -outputDir ..\..\..\results
```

## Ghi Chú

- Dataset lớn như `com-youtube` có thể chạy rất lâu.
- Luôn dùng cùng `-seedFile` cho mọi thuật toán trên cùng dataset.
- Luôn truyền `-outputDir ../../../results` cho IMin khi chạy từ `code/IMin/SandIMIN_code`.
- `prepare_dataset` chỉ còn dùng cho workflow legacy của SandIMIN, không bắt buộc cho dataset chung trong `datasets/`.
