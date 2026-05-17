# Influence Minimization via Node Blocking

Repo này chứa các baseline cho bài toán Influence Minimization:

| Algorithm | Code |
|---|---|
| AdvancedGreedy (AG) | `AdvancedGreedy/advanced_greedy.cpp` |
| GreedyReplace (GR) | `AdvancedGreedy/advanced_greedy.cpp` |
| IMin / SandIMIN | `IMin/SandIMIN_code/` |

Code đã được chỉnh để build được trên macOS và Windows. IMin không còn phụ thuộc `mmap`, `/proc/self/stat`, hoặc Unix-only headers khi build trên Windows.

## Dataset Format

Các dataset đầu vào chung nằm trong `datasets/` và có format:

```text
n m
directed|undirected
u1 v1
u2 v2
...
```

Xác suất cạnh dùng mô hình Weighted Cascade:

```text
p(u, v) = 1 / in_degree(v)
```

Seed được sinh bằng cách chọn ngẫu nhiên từ top-200 node theo out-degree. Nên dùng `-seedFile` để mọi thuật toán dùng cùng một tập seed.

## Run With A Fixed Seed Set

Khi đã có tập seed sinh trước, không nên để từng lần chạy tự sinh seed mới. Hãy dùng lại cùng một file seed cho mọi cấu hình trên cùng dataset.

### AdvancedGreedy / GreedyReplace

AdvancedGreedy đọc seed cố định bằng tham số `-seedFile`.

Ví dụ seed đã có:

```text
results/p2p-Gnutella31_seed_node_10.txt
```

macOS:

```bash
cd code/AdvancedGreedy
./ag -dataset ../../datasets/p2p-Gnutella31.txt -k 100 -seedNum 10 -theta 100 -mc 10000 -algo AG -seedFile ../../results/p2p-Gnutella31_seed_node_10.txt -output ../../results/ag_results.tsv
```

Windows PowerShell:

```powershell
cd code\AdvancedGreedy
.\ag.exe -dataset ..\..\datasets\p2p-Gnutella31.txt -k 100 -seedNum 10 -theta 100 -mc 10000 -algo AG -seedFile ..\..\results\p2p-Gnutella31_seed_node_10.txt -output ..\..\results\ag_results.tsv
```

Nếu file trong `-seedFile` tồn tại, chương trình sẽ load seed từ file đó. Nếu file chưa tồn tại, chương trình sẽ sinh seed mới và ghi ra file đó.

### IMin / SandIMIN

IMin không dùng tham số `-seedFile`. IMin đọc seed từ file:

```text
code/IMin/SandIMIN_code/dataset/<dataset-name>/rumorSet_<S>.txt
```

Trong đó `<S>` phải khớp với tham số `-rumorNum`.

Ví dụ với `-rumorNum 10`, IMin sẽ đọc:

```text
code/IMin/SandIMIN_code/dataset/p2p-Gnutella31/rumorSet_10.txt
```

Nếu đã có seed cố định từ AdvancedGreedy ở `results/p2p-Gnutella31_seed_node_10.txt`, có thể copy nội dung đó sang `rumorSet_10.txt`.

macOS:

```bash
cp results/p2p-Gnutella31_seed_node_10.txt code/IMin/SandIMIN_code/dataset/p2p-Gnutella31/rumorSet_10.txt

cd code/IMin/SandIMIN_code
./IMIN -dataset dataset/p2p-Gnutella31 -k 100 -rumorNum 10 -algo SandIMIN -epsilon 0.2 -gamma 0.1 -beta 0.1
```

Windows PowerShell:

```powershell
Copy-Item results\p2p-Gnutella31_seed_node_10.txt code\IMin\SandIMIN_code\dataset\p2p-Gnutella31\rumorSet_10.txt -Force

cd code\IMin\SandIMIN_code
.\IMIN.exe -dataset dataset\p2p-Gnutella31 -k 100 -rumorNum 10 -algo SandIMIN -epsilon 0.2 -gamma 0.1 -beta 0.1
```

Lưu ý: file seed chỉ cần chứa mỗi node một dòng. Thứ tự node không ảnh hưởng tới tập seed.

## Build On macOS

Yêu cầu:

- Xcode Command Line Tools hoặc Homebrew GCC/Clang.
- Terminal đang đứng ở thư mục `code/`.

Build từng chương trình:

```bash
cd AdvancedGreedy
g++ -O3 -std=c++17 -o ag advanced_greedy.cpp
g++ -O3 -std=c++17 -o experiment experiment.cpp

cd ../IMin
g++ -O3 -std=c++17 -o prepare_dataset prepare_dataset.cpp

cd SandIMIN_code
g++ -O3 -std=c++17 -o IMIN Sandwich.cpp sfmt/SFMT.c
```

Có thể chạy script tổng hợp trên macOS/Linux:

```bash
cd code
bash run_experiments.sh small
```

`run_experiments.sh` là shell script, không dùng trực tiếp trên Windows PowerShell.

## Build On Windows

Cách khuyến nghị là dùng MSYS2 MinGW-w64.

1. Cài MSYS2: https://www.msys2.org/
2. Mở terminal `MSYS2 MinGW x64`.
3. Cài compiler:

```bash
pacman -S --needed mingw-w64-x86_64-gcc
```

4. Đi tới thư mục `code/` của project, ví dụ:

```bash
cd /c/Users/<your-user>/path/to/ATBM/code
```

5. Build:

```bash
cd AdvancedGreedy
g++ -O3 -std=c++17 -o ag.exe advanced_greedy.cpp
g++ -O3 -std=c++17 -o experiment.exe experiment.cpp

cd ../IMin
g++ -O3 -std=c++17 -o prepare_dataset.exe prepare_dataset.cpp

cd SandIMIN_code
g++ -O3 -std=c++17 -o IMIN.exe Sandwich.cpp sfmt/SFMT.c -lpsapi
```

Ghi chú:

- `-lpsapi` cần cho phần đo memory trên Windows.
- Nếu dùng PowerShell thay vì MSYS2 shell, hãy dùng đường dẫn Windows như `..\..\datasets\p2p-Gnutella31.txt`.
- MSVC chưa phải target chính của repo này; dùng MinGW-w64 để tránh khác biệt compiler với mã gốc của SandIMIN.

## Prepare Dataset For IMin

IMin cần thư mục dataset riêng gồm `attribute.txt`, `graph.txt`, `graph_ic.inf`, và `rumorSet_<S>.txt`.

macOS:

```bash
cd code/IMin
./prepare_dataset -input ../../datasets/p2p-Gnutella31.txt -output SandIMIN_code/dataset/p2p-Gnutella31 -seedNum 50
```

Windows MSYS2:

```bash
cd code/IMin
./prepare_dataset.exe -input ../../datasets/p2p-Gnutella31.txt -output SandIMIN_code/dataset/p2p-Gnutella31 -seedNum 50
```

Windows PowerShell:

```powershell
cd code\IMin
.\prepare_dataset.exe -input ..\..\datasets\p2p-Gnutella31.txt -output SandIMIN_code\dataset\p2p-Gnutella31 -seedNum 50
```

## Run AdvancedGreedy / GreedyReplace

macOS:

```bash
cd code/AdvancedGreedy
./ag -dataset ../../datasets/p2p-Gnutella31.txt -k 100 -seedNum 10 -theta 100 -mc 10000 -algo AG -seedFile ../../results/p2p-Gnutella31_seed_node_10.txt -output ../../results/ag_results.tsv
```

Windows MSYS2:

```bash
cd code/AdvancedGreedy
./ag.exe -dataset ../../datasets/p2p-Gnutella31.txt -k 100 -seedNum 10 -theta 100 -mc 10000 -algo AG -seedFile ../../results/p2p-Gnutella31_seed_node_10.txt -output ../../results/ag_results.tsv
```

Windows PowerShell:

```powershell
cd code\AdvancedGreedy
.\ag.exe -dataset ..\..\datasets\p2p-Gnutella31.txt -k 100 -seedNum 10 -theta 100 -mc 10000 -algo AG -seedFile ..\..\results\p2p-Gnutella31_seed_node_10.txt -output ..\..\results\ag_results.tsv
```

Tham số chính:

| Parameter | Meaning |
|---|---|
| `-dataset` | Dataset format chung |
| `-k` | Số node cần chặn |
| `-seedNum` | Kích thước tập seed `|S|` |
| `-theta` | Số sampled graph cho AG/GR |
| `-mc` | Số Monte Carlo rounds để estimate spread |
| `-algo` | `AG`, `GR`, hoặc `BOTH` |
| `-seedFile` | File seed dùng chung |
| `-output` | File ghi kết quả dạng TSV |
| `-timeLimit` | Giới hạn thời gian, đơn vị giây |

## Run IMin / SandIMIN

macOS:

```bash
cd code/IMin/SandIMIN_code
mkdir -p results
./IMIN -dataset dataset/p2p-Gnutella31 -k 100 -rumorNum 10 -algo SandIMIN -epsilon 0.2 -gamma 0.1 -beta 0.1
```

Windows MSYS2:

```bash
cd code/IMin/SandIMIN_code
mkdir -p results
./IMIN.exe -dataset dataset/p2p-Gnutella31 -k 100 -rumorNum 10 -algo SandIMIN -epsilon 0.2 -gamma 0.1 -beta 0.1
```

Windows PowerShell:

```powershell
cd code\IMin\SandIMIN_code
New-Item -ItemType Directory -Force results
.\IMIN.exe -dataset dataset\p2p-Gnutella31 -k 100 -rumorNum 10 -algo SandIMIN -epsilon 0.2 -gamma 0.1 -beta 0.1
```

IMin ghi kết quả vào:

```text
code/IMin/SandIMIN_code/results/
```

Các dòng kết quả chính:

| Label | Meaning |
|---|---|
| `BEST` | Kết quả tốt nhất mà SandIMIN chọn |
| `LB` | Candidate lower-bound |
| `UB` | Candidate upper-bound |
| `OR` | Degree-based heuristic |

## Run Experiment Runner

`experiment` chạy AG và GR trên các dataset trong `datasets/`.

macOS:

```bash
cd code/AdvancedGreedy
./experiment -datadir ../../datasets -theta 100 -mc 10000 -timeLimit 100000 -log ../../results/ag_gr_results.tsv
```

Windows MSYS2:

```bash
cd code/AdvancedGreedy
./experiment.exe -datadir ../../datasets -theta 100 -mc 10000 -timeLimit 100000 -log ../../results/ag_gr_results.tsv
```

Windows PowerShell:

```powershell
cd code\AdvancedGreedy
.\experiment.exe -datadir ..\..\datasets -theta 100 -mc 10000 -timeLimit 100000 -log ..\..\results\ag_gr_results.tsv
```

Experiment mặc định:

- Vary `k = 100, 200, 300, 400, 500` với `|S| = 10`.
- Vary `|S| = 10, 20, 30, 40, 50` với `k = 100`.
- Tự skip nếu ước lượng runtime lớn hơn 1 ngày.

## Suggested Smoke Test

macOS:

```bash
mkdir -p results

cd code/AdvancedGreedy
./ag -dataset ../../datasets/p2p-Gnutella31.txt -k 1 -seedNum 10 -theta 2 -mc 20 -algo AG -seedFile ../../results/p2p-Gnutella31_seed_node_10.txt

cd ../IMin
./prepare_dataset -input ../../datasets/p2p-Gnutella31.txt -output SandIMIN_code/dataset/p2p-Gnutella31 -seedNum 50

cd SandIMIN_code
mkdir -p results
./IMIN -dataset dataset/p2p-Gnutella31 -k 1 -rumorNum 10 -algo SandIMIN -epsilon 0.2 -gamma 0.1 -beta 0.1
```

Windows PowerShell:

```powershell
New-Item -ItemType Directory -Force results

cd code\AdvancedGreedy
.\ag.exe -dataset ..\..\datasets\p2p-Gnutella31.txt -k 1 -seedNum 10 -theta 2 -mc 20 -algo AG -seedFile ..\..\results\p2p-Gnutella31_seed_node_10.txt

cd ..\IMin
.\prepare_dataset.exe -input ..\..\datasets\p2p-Gnutella31.txt -output SandIMIN_code\dataset\p2p-Gnutella31 -seedNum 50

cd SandIMIN_code
New-Item -ItemType Directory -Force results
.\IMIN.exe -dataset dataset\p2p-Gnutella31 -k 1 -rumorNum 10 -algo SandIMIN -epsilon 0.2 -gamma 0.1 -beta 0.1
```

## Notes

- Dataset lớn như `com-youtube` có thể chạy rất lâu.
- Để so sánh công bằng, giữ nguyên cùng `seedFile`/`rumorSet_<S>.txt` cho mọi thuật toán trên cùng dataset.
- `results/` ở repo root dùng cho báo cáo tổng hợp. IMin vẫn ghi raw output trong `code/IMin/SandIMIN_code/results/`.
