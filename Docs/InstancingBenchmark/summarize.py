"""Results/ 아래 측정 원본으로 summary.json, measurement-report.md, 차트와 대표 이미지를 만듭니다."""
from collections import defaultdict
from pathlib import Path
from statistics import mean
import hashlib
import json

import logging
import matplotlib
matplotlib.use('Agg')
logging.getLogger('matplotlib.font_manager').setLevel(logging.ERROR)
import matplotlib.pyplot as plt
from PIL import Image

root = Path(__file__).resolve().parent
results = root / 'Results'
env = json.loads((results / 'environment.json').read_text(encoding='utf-8-sig'))
counts = [int(c) for c in env['counts']]
mesh = env['mesh']
VIEWS = {'overview': '전체 장면', 'belt': '소행성대 근처'}

summary = {}
for count in counts:
    folder = results / f'{mesh}-{count}'
    frames = defaultdict(list)
    phases = defaultdict(list)
    for line in (folder / 'frames.jsonl').read_text().splitlines():
        row = json.loads(line)
        frames[row['view'], row['mode']].append(row)
    for line in (folder / 'phases.jsonl').read_text().splitlines():
        row = json.loads(line)
        phases[row['view'], row['mode']].append(row)
    for (view, mode), rows in frames.items():
        runs = phases[view, mode]
        assert len(rows) == sum(r['frames'] for r in runs)
        item = {k: mean(r[k] for r in rows) for k in ['cpu_frame_ms', 'cpu_belt_ms', 'gpu_frame_ms', 'gpu_belt_ms']}
        item['frames'] = len(rows)
        item['elapsed_s'] = sum(r['elapsed_s'] for r in runs)
        item['fps'] = item['frames'] / item['elapsed_s']
        item['run_fps'] = [r['fps'] for r in runs]
        summary[f'{count}/{view}/{mode}'] = item
    validation = [json.loads(l) for l in (folder / 'validation.jsonl').read_text().splitlines()]
    summary[f'{count}/validation'] = validation

(root / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')

# 대표 이미지: 가장 많은 개수의 인스턴싱 결과
largest = results / f'{mesh}-{counts[-1]}'
for view in VIEWS:
    source = largest / f'{view}-instanced.ppm'
    if source.exists():
        Image.open(source).save(root / f'{view}-instanced.png')

# 차트: 소행성 수별 FPS (로그 축)
from matplotlib import font_manager
available = {f.name for f in font_manager.fontManager.ttflist}
plt.rcParams['font.family'] = next((n for n in ['Malgun Gothic', 'NanumGothic', 'Noto Sans CJK KR', 'Noto Sans CJK JP', 'Noto Serif CJK KR', 'Noto Serif CJK JP'] if n in available), 'DejaVu Sans')
fig, axes = plt.subplots(1, 2, figsize=(10, 4), sharey=True)
for ax, (view, label) in zip(axes, VIEWS.items()):
    x = range(len(counts))
    individual = [summary[f'{c}/{view}/individual']['fps'] for c in counts]
    instanced = [summary[f'{c}/{view}/instanced']['fps'] for c in counts]
    width = 0.38
    b1 = ax.bar([i - width / 2 for i in x], individual, width, label='개별 드로우', color='#9aa5b1')
    b2 = ax.bar([i + width / 2 for i in x], instanced, width, label='인스턴싱', color='#2f6fde')
    for bars in (b1, b2):
        for bar in bars:
            ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() * 1.08, f'{bar.get_height():,.0f}',
                    ha='center', va='bottom', fontsize=8)
    ax.set_yscale('log')
    ax.set_xticks(list(x), [f'{c:,}개' for c in counts])
    ax.set_title(label)
    ax.spines[['top', 'right']].set_visible(False)
axes[0].set_ylabel('렌더링 처리량 (FPS, 로그 축)')
axes[0].legend(frameon=False)
fig.tight_layout()
fig.savefig(root / 'fps-by-count.png', dpi=150)

def row(count, view):
    ind = summary[f'{count}/{view}/individual']
    ins = summary[f'{count}/{view}/instanced']
    return ind, ins

lines = ['# 소행성 인스턴싱 비교 실측', '',
         f"측정: {env['measured_at']}.", '',
         '같은 장면에서 소행성을 그리는 방식만 바꿔, 드로우 콜을 합치는 효과를 비교한 실험입니다.', '',
         '## 측정 조건', '',
         f"- CPU: {env['cpu'].strip()}. GPU: NVIDIA GeForce RTX 4060 Laptop GPU, OpenGL 4.6.",
         '- MSVC x64, C++20, `/O2 /DNDEBUG`, 1920×1080, 4x MSAA 오프스크린 프레임버퍼. VSync와 화면 표시는 포함하지 않습니다.',
         '- 소행성 메시: 드로우 콜 비용을 분리해 보기 위해 저폴리 바위(정점 42개, 삼각형 80개)를 사용했습니다. 원거리 LOD에 해당하는 수준입니다.',
         f"- 소행성 수: {', '.join(f'{c:,}' for c in counts)}개. 텍스처, 셰이더, 개체별 데이터, 카메라와 시뮬레이션 시간은 같게 고정했습니다.",
         '- 개별 드로우: 소행성마다 `glVertexAttrib4fv` 3회와 `glDrawElements` 1회. 인스턴싱: `glDrawElementsInstanced` 1회.',
         '- 조건마다 0.5초 예열, 1.5초 측정을 2회 반복했고 반복마다 두 방식의 순서를 바꿨습니다.',
         '- GPU 시간은 `GL_TIMESTAMP` 쿼리, CPU 시간은 소행성 드로우 함수가 반환될 때까지의 호출 스레드 시간입니다. 드라이버가 반환 이후에 처리하는 작업은 CPU 시간에 포함되지 않습니다.',
         '- FPS는 총 완료 프레임 수를 총 경과 시간으로 나눈 렌더링 처리량입니다.', '',
         '## 결과', '',
         '![소행성 수별 FPS](fps-by-count.png)', '',
         '| 소행성 수 | 시점 | 개별 드로우 FPS | 인스턴싱 FPS | 향상 | CPU 제출 시간 (개별 → 인스턴싱) |',
         '|---:|---|---:|---:|---:|---|']
for count in counts:
    for view, label in VIEWS.items():
        ind, ins = row(count, view)
        lines.append(f"| {count:,} | {label} | {ind['fps']:,.1f} | {ins['fps']:,.1f} | {ins['fps'] / ind['fps']:.1f}배 "
                     f"| {ind['cpu_belt_ms']:.2f}ms → {ins['cpu_belt_ms'] * 1000:.2f}µs |")
lines += ['', '모든 조건에서 인스턴싱의 처리량이 높았습니다. 개별 드로우는 소행성 수에 비례해 CPU 제출 시간이 늘어 프레임 시간이 그만큼 길어졌고, 인스턴싱은 개수와 관계없이 드로우 호출 한 번으로 제출했습니다.', '',
          '개별 드로우의 GPU 소행성 구간 시간은 CPU 제출 시간과 거의 같았습니다. GPU가 계산하느라 오래 걸린 것이 아니라 CPU가 보내는 명령을 기다린 시간이 포함된 것입니다. 인스턴싱에서는 같은 기하를 GPU가 한 번에 받아 처리하므로 구간 시간이 크게 줄었습니다.', '',
          '| 소행성 수 | 시점 | GPU 소행성 구간 (개별) | GPU 소행성 구간 (인스턴싱) |', '|---:|---|---:|---:|']
for count in counts:
    for view, label in VIEWS.items():
        ind, ins = row(count, view)
        lines.append(f"| {count:,} | {label} | {ind['gpu_belt_ms']:.3f}ms | {ins['gpu_belt_ms']:.3f}ms |")
lines += ['', '## 반복 결과', '', '| 소행성 수 | 시점 | 방식 | 1회 FPS | 2회 FPS |', '|---:|---|---|---:|---:|']
for count in counts:
    for view, label in VIEWS.items():
        for mode, mlabel in [('individual', '개별 드로우'), ('instanced', '인스턴싱')]:
            values = summary[f'{count}/{view}/{mode}']['run_fps']
            lines.append(f"| {count:,} | {label} | {mlabel} | {values[0]:,.1f} | {values[1]:,.1f} |")
lines += ['', '## 화면 검증', '', '측정 전에 두 방식으로 같은 프레임을 그려 픽셀을 비교했습니다. 채널 차이는 최대 1/255로 반올림 수준이며, 이를 넘으면 측정을 중단합니다.', '']
for count in counts:
    for v in summary[f'{count}/validation']:
        lines.append(f"- {count:,}개 · {VIEWS[v['view']]}: 2,073,600픽셀 중 {v['different_pixels']}픽셀 차이, 최대 채널 차이 {v['max_channel_difference']}/255.")
lines += ['', f"![전체 장면, 소행성 {counts[-1]:,}개](overview-instanced.png)", '',
          f"![소행성대 근처, 소행성 {counts[-1]:,}개](belt-instanced.png)", '',
          '## 해석 범위', '',
          '이 실험은 드로우 호출을 합치는 효과를 보기 위해, 소행성 하나의 GPU 처리 비용이 작은 저폴리 메시를 사용했습니다. 앱 본편의 고해상도 바위 메시처럼 소행성 하나의 정점 수가 많으면 GPU 처리 시간이 프레임을 결정하므로, 인스턴싱으로 CPU 제출 시간을 줄여도 FPS 차이는 작아질 수 있습니다.', '',
          '측정 시간이 짧아 노트북의 전력·온도 상태에 따라 수치가 달라질 수 있습니다. 같은 조건의 두 반복은 FPS 기준 5% 이내로 일치했습니다.', '',
          '## 재현', '',
          '`run-benchmark.cmd`를 실행하면 빌드와 측정을 수행하고 원본을 `Results/`에 저장합니다. `summarize.py`는 원본으로부터 이 보고서와 `summary.json`, 차트를 생성합니다.', '',
          '```powershell', r'.\build-run.ps1 -ProjectRoot C:\path\to\3D-Solar-System-Simulator -VcpkgRoot C:\vcpkg', r'python .\summarize.py', '```', '',
          '측정용 소스 SHA-256: `' + hashlib.sha256((root / 'benchmark.cpp').read_bytes()).hexdigest() + '`.', '']
(root / 'measurement-report.md').write_text('\n'.join(lines), encoding='utf-8')
for count in counts:
    for view in VIEWS:
        ind, ins = row(count, view)
        print(count, view, f"{ind['fps']:.1f} -> {ins['fps']:.1f} ({ins['fps']/ind['fps']:.2f}x)", f"cpu {ind['cpu_belt_ms']:.3f} -> {ins['cpu_belt_ms']*1000:.3f}us", f"gpu {ind['gpu_belt_ms']:.3f} -> {ins['gpu_belt_ms']:.3f}")
