from collections import defaultdict
from pathlib import Path
from statistics import mean
import hashlib
import json
from PIL import Image

root = Path(__file__).resolve().parent
groups = defaultdict(list)
phases = defaultdict(list)
for line in (root / 'frames.jsonl').read_text().splitlines():
    row = json.loads(line)
    groups[row['view'], row['mode']].append(row)
for line in (root / 'phases.jsonl').read_text().splitlines():
    row = json.loads(line)
    phases[row['view'], row['mode']].append(row)

summary = {}
for key, rows in groups.items():
    runs = phases[key]
    assert len(runs) == 3
    assert len(rows) == sum(r['frames'] for r in runs)
    stats = {name: mean(r[name] for r in rows) for name in ['cpu_frame_ms', 'cpu_belt_ms', 'gpu_frame_ms', 'gpu_belt_ms']}
    stats['frames'] = len(rows)
    stats['elapsed_s'] = sum(r['elapsed_s'] for r in runs)
    stats['fps'] = stats['frames'] / stats['elapsed_s']
    stats['run_fps'] = [r['fps'] for r in runs]
    stats['gpu_belt_run_means'] = [mean(r['gpu_belt_ms'] for r in rows if r['run'] == i) for i in range(1, 4)]
    stats['cpu_belt_run_means'] = [mean(r['cpu_belt_ms'] for r in rows if r['run'] == i) for i in range(1, 4)]
    summary['/'.join(key)] = stats

(root / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
for source in root.glob('*.ppm'):
    Image.open(source).save(source.with_suffix('.png'))
    source.unlink()

lines = ['# 소행성 인스턴싱 비교 실측', '',
         '측정일: 2026-10-18 (한국 시간).', '',
         '현재 구현에서 드로우 방식만 바꿔 재현한 비교 실험입니다. 개발 당시의 변경 전후 기록은 아닙니다. 기존 README의 수치와 설명은 사용하지 않았습니다.', '',
         '## 측정 조건', '',
         '- 소스: `JuggernautSensei/3D-Solar-System-Simulator`, commit `0a35574340bd9a171c0a023fb5215e7f9ac9faf1`.',
         '- CPU: AMD Ryzen 9 7945HX. GPU: NVIDIA GeForce RTX 4060 Laptop GPU. OpenGL 4.6, NVIDIA 드라이버 617.14.',
         '- MSVC x64, C++20, `/O2 /DNDEBUG`, 1920×1080, 4x MSAA, RGBA8 + depth24 오프스크린 프레임버퍼.',
         '- 소행성 4,000개. 메시, 텍스처, 셰이더, 개체별 데이터, 카메라와 시뮬레이션 시간(`1600`일)을 동일하게 고정했습니다.',
         '- 전체 장면 카메라: 위치 `(0, 68, 73)`, yaw `0`, pitch `-43`. 소행성대 카메라: 위치 `(0, 1.5, 29)`, yaw `0`, pitch `-5`.',
         '- 개별 드로우: 인스턴스 속성 배열을 끄고 소행성마다 `glVertexAttrib4fv` 3회와 `glDrawElements` 1회를 호출합니다. 인스턴싱: 속성 배열을 사용하고 `glDrawElementsInstanced` 1회로 그립니다.',
         '- 개별 드로우용 데이터는 인스턴스 생성 시 CPU 벡터에서 복사했습니다. 측정 준비 중 GPU 인스턴스 버퍼를 읽어오는 경로는 사용하지 않습니다.',
         '- 시점별·방식별 3회. 매회 3초 예열 후 8초 이상 측정했으며 방식의 실행 순서는 매 반복마다 바꿨습니다.',
         '- GPU 시간은 64비트 `GL_TIMESTAMP` 쿼리로 전체 장면과 소행성 구간을 각각 측정했습니다. 쿼리는 16슬롯을 순환하며 지연 수집했습니다.',
         '- CPU 소행성 시간은 위 드로우 함수가 반환될 때까지의 호출 스레드 시간입니다. 드라이버 내부 작업 전체나 순수한 계산량과 같지는 않습니다.',
         '- FPS는 GPU 완료 대기까지 포함한 총 경과 시간 대비 완료 프레임 수입니다. 화면 표시, VSync와 창 합성은 포함하지 않는 렌더링 처리량입니다.', '',
         '## 결과', '',
         '| 시점 | 방식 | 렌더링 처리량(FPS) | CPU 소행성 구간(ms) | GPU 소행성 구간(ms) | GPU 전체 장면(ms) |',
         '|---|---|---:|---:|---:|---:|']
for view in ['overview', 'belt']:
    for mode in ['individual', 'instanced']:
        item = summary[f'{view}/{mode}']
        lines.append(f"| {'전체 장면' if view == 'overview' else '소행성대 근처'} | {'개별 드로우 4,000회' if mode == 'individual' else '인스턴싱 1회'} | {item['fps']:.2f} | {item['cpu_belt_ms']:.4f} | {item['gpu_belt_ms']:.3f} | {item['gpu_frame_ms']:.3f} |")
lines += ['', 'CPU의 소행성 드로우 호출 구간 시간은 크게 줄었습니다. GPU 구간 시간은 줄지 않았고, 두 시점 모두 FPS 향상은 관찰되지 않았습니다. 이번 실험은 인스턴싱의 CPU 호출 비용 감소를 보여 주지만, FPS를 개선했다는 결론을 뒷받침하지는 않습니다.', '',
          '시간은 모든 측정 프레임의 산술평균이며 FPS는 3회 측정의 총 프레임 수를 총 경과 시간으로 나눈 값입니다.', '',
          '## 반복 결과', '', '| 시점 | 방식 | 1회 FPS | 2회 FPS | 3회 FPS |', '|---|---|---:|---:|---:|']
for view in ['overview', 'belt']:
    for mode in ['individual', 'instanced']:
        values = summary[f'{view}/{mode}']['run_fps']
        lines.append(f"| {view} | {mode} | {values[0]:.2f} | {values[1]:.2f} | {values[2]:.2f} |")
lines += ['', '## 화면 검증', '']
for line in (root / 'validation.jsonl').read_text().splitlines():
    row = json.loads(line)
    lines.append(f"- {row['view']}: 총 2,073,600픽셀 중 {row['different_pixels']}픽셀 차이, 최대 채널 차이 {row['max_channel_difference']}/255. 미세한 수치 차이를 허용했습니다(최대 1/255, 2,000픽셀 이내).")
lines += ['', '![전체 장면](overview-instanced.png)', '', '![소행성대 근처](belt-instanced.png)', '',
          '## 해석 범위', '',
          '이 실험은 같은 셰이더에 같은 개체별 데이터를 전달하면서 드로우를 합치는 효과를 비교합니다. 당시 사용했던 과거 렌더러를 복원한 실험은 아닙니다. CPU에서 개체별 행렬까지 계산하는 구현과의 비교도 아닙니다. 같은 셰이더를 사용해 두 방식의 계산량을 맞췄습니다.', '',
          '기하 복잡도와 소행성 수를 줄이지 않았으므로, 인스턴싱이 GPU의 삼각형 처리량 자체를 줄였다고 해석할 수 없습니다. CPU 호출 시간에는 드라이버의 대기나 큐 제어가 영향을 줄 수 있습니다. 노트북 전력·온도·다른 프로그램의 부하도 결과에 영향을 줍니다.', '',
          '측정 도구를 검증하던 예비 실행에서 예열 중 과도한 작업 제출과 GPU 인스턴스 버퍼 읽기 이후의 비정상적인 성능 저하를 확인했습니다. 예열에서는 매 프레임 GPU 완료를 기다리도록 하고, 비교용 데이터는 CPU 원본에서 복사하도록 고친 뒤 전체 측정을 다시 수행했습니다. 위 표와 원본 데이터에는 수정 후 완료한 정식 실행만 포함했습니다.', '',
          '프레임별 원본은 `frames.jsonl`, 반복별 처리량은 `phases.jsonl`, 장치 및 실행 로그는 `run.log`, 화면 검증은 `validation.jsonl`에 있습니다. 측정용 전체 소스는 `benchmark.cpp`이고 `build-run.ps1`로 빌드·실행할 수 있습니다. `summarize.py`는 원본으로부터 이 보고서와 `summary.json`을 생성합니다.', '',
          '```powershell', r'.\build-run.ps1 -ProjectRoot C:\path\to\3D-Solar-System-Simulator -VcpkgRoot C:\vcpkg', r'python .\summarize.py', '```', '',
          '측정용 소스 SHA-256: `' + hashlib.sha256((root/'benchmark.cpp').read_bytes()).hexdigest() + '`.', '']
(root / 'measurement-report.md').write_text('\n'.join(lines), encoding='utf-8')
print(json.dumps(summary, ensure_ascii=False, indent=2))
