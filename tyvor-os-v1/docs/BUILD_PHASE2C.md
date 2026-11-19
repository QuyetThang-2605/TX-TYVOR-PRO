# Phase 2C software-only build / replay

Ngày 10/11/2026. Không command nào bên dưới mở serial/debugger, flash, cấp nguồn, đổi gate hoặc bật RF. SDK có thể tự in hướng dẫn flash trong build log; không chạy các gợi ý đó.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-phase2c.ps1 -Target all
.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/test_phase2c_guards.py
.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/test_phase2c_platform.py
.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/probe_timing_regression.py --config Debug --output build/phase2c/evidence
.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/probe_timing_regression.py --config Release --output build/phase2c/evidence
.tools/espressif/python_env/idf5.5_py3.11_env/Scripts/python.exe scripts/phase2c_artifacts.py inspect
.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/phase2c_artifacts.py package
$env:TYVOR_BUILD_PHASE='phase2c'
.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/test_package_validation.py --report build/phase2c/evidence/package-tests.json
.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/phase2c_artifacts.py package
.tools/python/cpython-3.11.13-windows-x86_64-none/python.exe scripts/validate_esp_package.py artifacts/phase2c/esp32 --manifest artifacts/phase2c/SHA256_MANIFEST.json --workspace . --report build/phase2c/evidence/final-manifest-validation.json
```

Managed toolchain/SDK versions giữ nguyên Phase 2B. Build tách riêng `build/phase2c`; package `artifacts/phase2c`, SVG `artifacts/phase2c-ui`. Four build receipts bind actual compiler/source/config/output/JUnit; packager kiểm tra đúng 177 case mỗi cấu hình, NDEBUG, no skips, current memory inspection, platform-helper source hashes và 35 view hashes. Package cũ nếu có được giữ trong `build/phase2c-baseline/package-*`. Phase1/2A/2B artifacts giữ nguyên.

Baseline trước sửa: `build/phase2c-baseline/source/` (108 file), `PHASE2B_MANIFEST.json`, Debug/Release154-case JUnit. Probe BSP trên frozen source: thêm `--source build/phase2c-baseline/source` vào lệnh probe trên, output ở baseline; **expected exit1** do fake HAL dùng đúng timeout10 ms trong function trích nguyên văn. Current probe expected exit0; không phải phép đo wall time/board.

Regression bổ sung trong quá trình phát triển queue: `tx_deadline_before_driver`. Source trước sửa ở `build/phase2c-baseline/async-poll-before.c`; log `development-deadline-{Debug,Release}-before.txt` expected FAIL. Final CTest và `*-after.txt` PASS. Không gán lỗi mới của queue cho baseline Phase2B chưa có queue.

`test_phase2c_platform.py` trích chính helper ESP FIFO và STM ISR/start/abort hiện hành rồi compile cùng register/FIFO fakes ở cả Debug và NDEBUG. Không compile toàn bộ HAL/FreeRTOS thành simulator và không chứng minh ISR/RTOS timing thật. STM gated branch được Arm cross-compile thành object để type/API check, không link vào default ELF.

`fetch_phase2c_board_sources.py` chỉ tải tài liệu vào evidence; cần network riêng, không là bước build firmware. Schematic/hash/tree snapshot đã lưu trong package. PDF được trích text/render bằng PyMuPDF1.26.6 cài riêng dưới `build/phase2c-doc-tools`, không đổi Python global/SDK. Không cần PDF tool cho build/test/replay firmware.
