# TAC-OPS: Dam Valley (Unreal Engine 5.8)

Unity 6로 만들었던 1인칭 FPS를 **Unreal Engine 5.8 C++ 프로젝트**로 옮겨 다시 만들고, **델타포스(Delta Force, 2024)** 의 그래픽·게임성·요소·맵 구성에 최대한 가깝게 확장한 프로젝트입니다.

- 엔진: Unreal Engine **5.8** (`TacOps.uproject` → `EngineAssociation: 5.8`)
- 언어: C++ 100% (블루프린트·바이너리 에셋 없이 동작)
- 모드: **Operations**(위험 작전형 PvPvE 탈출)와 **Warfare**(공격/방어 대규모 전투)
- 맵: **Dam Valley**(2.5 km × 2.5 km, 12개 거점, 탈출구 6곳, 전선 섹터 4개). 실행할 때 절차적으로 생성됩니다.

> **원본 Unity 코드에 대해**
> 이전 "Unity 6 1인칭 FPS" 채팅의 코드는 이 세션에서 열 수 없었습니다. 저장소가 비어 있었고 다른 세션 기록도 없었습니다.
> 그래서 일반적인 Unity FPS 구성(플레이어 이동/마우스룩, 총기, 적 AI, 게임 매니저, UI)을 기준으로 이식했습니다.
> 원본 Unity 스크립트(`*.cs`)를 이 저장소에 올려 주시면 수치·동작을 그대로 맞춰 다시 이식할 수 있습니다.
> 아래 "Unity → Unreal 대응표"에 어느 스크립트가 어느 클래스로 가는지 정리했습니다.

---

## 1. 실행 방법

### 준비물
- Unreal Engine 5.8 (Epic Games Launcher)
- Windows: Visual Studio 2022(“C++를 사용한 게임 개발” 워크로드). Mac/Linux: 엔진 권장 툴체인.
- DX12 / Shader Model 6 지원 GPU(Lumen, Nanite, Virtual Shadow Maps 사용)

### 빌드
1. `TacOps.uproject` 우클릭 → **Generate Visual Studio project files**
2. `TacOps.sln` 열기 → 구성 `Development Editor` / `Win64` → **빌드**
3. `TacOps.uproject` 실행(또는 VS에서 F5)

> 다른 5.x 버전을 쓴다면 `.uproject`를 우클릭 → *Switch Unreal Engine version* 으로 바꾸면 됩니다.

### 첫 실행 시 머티리얼 자동 생성
에디터를 처음 열면 `Content/Python/init_unreal.py`가 실행되어 다음 머티리얼을 만듭니다(Python Editor Script Plugin 필요, `.uproject`에서 이미 활성화됨).

`/Game/TacOps/Materials/M_TO_Master`, `M_TO_Glass`, `M_TO_Water`, `M_TO_Smoke`, `M_TO_Unlit`

- 다시 만들려면: *Tools → Execute Python Script →* `Tools/setup_assets.py` (인자 `force`)
- 이 머티리얼이 없어도 게임은 엔진 기본 머티리얼로 실행됩니다. 다만 월드 색 변화, 금속/거칠기 표현, 유리·물·연기의 반투명이 빠져서 그래픽이 단순해집니다.

### 플레이
- **Play → Standalone Game**을 권장합니다. 에디터 PIE에서는 `Esc`가 플레이를 종료하므로, PIE에서는 일시정지에 **P** 키를 쓰세요.
- 게임은 엔진의 빈 `Entry` 맵에서 시작합니다. `TOGameMode`가 지형, 강, 댐, 건물, 식생, 조명을 전부 런타임에 생성합니다. 로딩 화면이 끝나면 로비가 나옵니다.

---

## 2. 조작법 (델타포스 PC 기본 배치 기준)

| 키 | 동작 |
|---|---|
| W A S D / 마우스 | 이동 / 시점 |
| 좌클릭 / 우클릭 | 사격 / 조준(ADS) |
| Shift | 달리기, 스코프 중에는 숨 참기 |
| Space | 점프 / 넘기(볼트) |
| C, Ctrl | 앉기(달리는 중이면 슬라이딩) |
| Z | 엎드리기 |
| Q / E | 좌·우 기울이기(린) |
| R | 재장전 |
| F (홀드) | 상호작용: 상자 수색, 문/키카드, 무전기, 결제 단말기, 아군 부활 |
| 1 2 3 4 / 휠 | 주무기 / 보조 주무기 / 권총 / 나이프, 무기 순환 |
| B | 사격 모드(단발/점사/연사) |
| G / 5 | 수류탄 투척 / 투척물 종류(파편·연막·섬광) |
| X | 오퍼레이터 스킬 |
| H | 빠른 치료(상태에 맞는 의료품 자동 선택) |
| T / N | 전술 라이트 / 야간투시경 |
| 휠 클릭 | 핑: 적 스폿, 분대 이동 지시 |
| Tab, I | 인벤토리(그리드) / 루팅 |
| M | 전술 지도(클릭하면 마커 표시) |
| Esc, P | 일시정지 메뉴 |

마우스 감도, 조준 감도, 시야각(FOV), 그래픽 품질, 볼륨, 상하 반전, 조준 토글, FPS 표시는 **설정** 화면에서 바꿉니다. 설정은 세이브에 저장됩니다.

---

## 3. 게임 모드

### Operations (위험 작전 / 탈출)
- 로비에서 오퍼레이터와 장비를 고르고 **크레딧을 내고 출격**합니다. 돈이 없으면 **무료 키트**(MP5, 2레벨 방탄복)로 출격할 수 있습니다.
- 맵 곳곳의 상자, 금고, 서버랙, 무기고, 시체를 **수색**하면 아이템이 하나씩 공개됩니다. 희귀도(흰/초록/파랑/보라/금/빨강)에 따라 공개 시간과 효과음이 달라집니다.
- 거점마다 경비 AI가 있습니다. 행정동 지휘관(보스)이 있고, **라이벌 오퍼레이터 분대**가 시간차로 투입되어 같은 건물을 털고 탈출합니다.
- **탈출구 6곳**(매 판 일부만 열림):
  - **상시**: South Checkpoint, River Boat, Mountain Pass
  - **무전 호출**: Base Helipad. 무전기를 켜면 헬기가 옵니다. 소리가 크게 납니다.
  - **결제**: Dam Cable Car. 60,000 크레딧
  - **가방 금지**: Rail Tunnel. 인벤토리에서 백팩을 버려야 통과합니다.
- 살아서 탈출하면 가지고 나온 모든 것이 판매되어 크레딧이 됩니다. 죽으면 장비를 모두 잃습니다. **보안 컨테이너(3×2)** 에 넣은 물건만 남습니다.
- 키카드 방:
  - 행정동 4층 국장실 = 파란 키카드
  - 별관 2층 서버 금고 = 빨간 키카드
  - 무기고 벙커 = 무기고 키

### Warfare (전면전 / 공격·방어)
- 12 대 12(플레이어 + 봇 11명). 섹터 4개 × 거점 2개(A/B). 공격팀 티켓 250장.
- 섹터: Old Village → River Crossing → Admin & Base → Hydro Dam
- 공격팀은 사망할 때마다 티켓이 1장 줄어듭니다. 두 거점을 모두 점령하면 다음 섹터로 넘어가고 티켓 +75를 받습니다.
- 사망하면 잠시 뒤 **재배치 화면**이 나옵니다. 오퍼레이터와 무기 프리셋을 고르고, 사망 6초 후부터 재투입할 수 있습니다.
- 처치, 점령, 승리 보상은 크레딧으로 들어옵니다.

---

## 4. 델타포스 요소 대응표

| 델타포스 요소 | TAC-OPS 구현 |
|---|---|
| Hazard Operations(탈출형 PvPvE) | `TOGameMode_Ops.cpp`: 레이드 타이머 25분, 경비 AI, 보스, 라이벌 분대, 조건부 탈출, 경제 |
| Havoc Warfare(대규모 공방) | `TOGameMode_Warfare.cpp`: 섹터/거점/티켓, 봇 리스폰, 재배치 |
| 오퍼레이터 & 스킬 | VIPER(돌격·Adrenal Surge), HALO(지원·Heal Pulse, 빠른 부활), BASTION(공병·Deploy Shield), ECHO(정찰·Recon Pulse) |
| 방탄복/헬멧 레벨 1–6, 내구도, 탄 관통 등급 | `TOHealthComponent`: 부위별 배율, 방어구 레벨과 탄 등급 차이표, 내구도 감소 |
| 부위 피해, 출혈, 골절, 진통제, 기절/부활 | `TOHealthComponent` + `TOCharacter` |
| 그리드 인벤토리(주머니/조끼/가방/보안 컨테이너) | `TOInventoryComponent`, `TOHUD_Inventory.cpp`(우클릭 빠른 이동, 컨텍스트 메뉴, 툴팁) |
| 단계별 수색과 희귀도 공개 | `TOLootContainer`: 아이템마다 공개 시간, 희귀 아이템 효과음 |
| 건스미스(부착물 7슬롯) | 무기 23종, 부착물 22종, 스탯 비교 그래프(`TOHUD_Menus.cpp`) |
| 탄도 | 탄속, 중력 낙차, 거리별 피해 감소, 예광탄, 근접 탄 소리/제압 효과(`TOCombatManager`) |
| 반동/조준 | 수직·수평 반동 임펄스와 회복, ADS 줌, 스코프 숨 흔들림, 숨 참기 |
| 이동 | 달리기/스태미나, 슬라이딩, 넘기, 린, 엎드리기, 수영, 낙하 피해 |
| 키카드/잠긴 문 | `TODoor` |
| HUD | 상단 나침반, 미니맵, 체력·방어구 바, 탄약·사격 모드·탄 등급, 킬피드, 히트마커(머리/방어구/처치 구분), 피격 방향, 탈출 진행 바 |
| 시간대 | 낮 / 해질녘 / 밤(야간투시경 지원) |

---

## 5. 그래픽 (델타포스 스타일)

- **Lumen** GI와 반사, **Nanite**, **Virtual Shadow Maps**, **TSR**, DX12/SM6, MegaLights
- SkyAtmosphere, 실시간 SkyLight, **볼류메트릭 구름**, **볼류메트릭 포그**
- 델타포스풍 컬러 그레이딩: 약간 차가운 그림자, 따뜻한 하이라이트, 낮은 채도, 필름 그레인, 비네트. 낮/해질녘/밤마다 노출, 안개, 그레이드가 다릅니다(`TOEnvironment`).
- 지형: 2.5 km 프로시저럴 메쉬(재질 섹션별 분리, 비동기 콜리전). 강, 저수지, 도로, 다리, 송전탑, 댐, 발전소, 레이더 기지, 군사기지, 마을, 숲이 들어갑니다.
- 수천 개 오브젝트를 **HISM(계층형 인스턴스 메쉬)** 으로 묶어 드로우콜을 줄였습니다.

### 실사 에셋으로 업그레이드하기
모든 표면은 이름이 붙은 재질 ID로 관리됩니다. `/Game/TacOps/Materials/Overrides/MO_<이름>` 에 머티리얼을 만들면 그 머티리얼이 자동으로 쓰입니다. 예를 들어 Quixel **Megascans** 머티리얼을 쓸 수 있습니다.

- 예: `MO_Grass`, `MO_Rock`, `MO_Asphalt`, `MO_Concrete`, `MO_Brick`, `MO_MetalRust`, `MO_FoliagePine`, `MO_WaterRiver`
- 이름 목록: `Source/TacOps/World/TOMaterialLibrary.cpp`의 `GetInfo()`

---

## 6. Unity → Unreal 대응표

| Unity (일반적인 FPS 구성) | Unreal (이 프로젝트) |
|---|---|
| `PlayerMovement.cs` / `FirstPersonController.cs` | `ATOCharacter` (CharacterMovement, 자세, 린, 넘기, 슬라이딩, 스태미나) |
| `MouseLook.cs` | `ATOPlayerController::OnLook` + 캐릭터 카메라 |
| `Input Manager` / `PlayerInput` | Enhanced Input(코드에서 액션/매핑 생성, 에셋 불필요) |
| `Gun.cs` / `WeaponController.cs` | `UTOWeaponComponent` + `UTOWeaponVisuals` |
| `Bullet.cs` / Raycast 사격 | `ATOCombatManager`(탄도 시뮬레이션, 이펙트 풀) |
| `PlayerHealth.cs` / `EnemyHealth.cs` | `UTOHealthComponent` |
| `EnemyAI.cs` (NavMeshAgent) | `ATOAIController`(상태 머신, 인지, 엄폐, 분대, 런타임 내비메쉬) |
| `GameManager.cs` / `WaveSpawner.cs` / `ScoreManager.cs` | `ATOGameMode`(+ `_Ops.cpp`, `_Warfare.cpp`) |
| `Inventory.cs` | `UTOInventoryComponent` |
| `UIManager.cs` / `HUD.cs` / `MainMenu.cs` | `ATOHUD`(캔버스 기반 HUD와 전체 메뉴, UMG 에셋 불필요) |
| `PlayerPrefs` | `UTOSaveGame` + `UTOGameInstance` |
| `AudioSource` / `AudioClip` | `UTOAudio`(PCM 실시간 합성, 사운드 에셋 불필요) |
| Scene / Terrain | `ATOWorldGenerator`(Dam Valley 절차적 생성) |

---

## 7. 소스 구조

```
Source/TacOps/
  Core/        타입, 데이터베이스(아이템/무기/부착물/오퍼레이터), 게임모드, 컨트롤러, 세이브, 인스턴스
  Characters/  캐릭터, 체력, 인벤토리, 절차적 병사 몸체(히트박스)
  Weapons/     무기 컴포넌트, 총기 비주얼, 탄도/이펙트 매니저, 수류탄, 방패
  AI/          AI 컨트롤러
  World/       월드 생성기(지형, 프리팹, Dam Valley 레이아웃), 환경(하늘/조명/안개), 루팅 상자, 문, 탈출/점령 지점, 머티리얼 라이브러리
  UI/          HUD(전투 HUD, 지도, 인벤토리, 메뉴)
  Audio/       절차적 사운드
Content/Python/  에디터 시작 시 머티리얼 생성 스크립트
Tools/           수동 에셋 생성 스크립트
Config/          렌더링(Lumen/Nanite/VSM), 입력, 콜리전 채널(Bullet), 내비게이션 설정
```

---

## 8. 현재 한계와 다음 단계

- **아트**: 캐릭터, 총기, 건물은 엔진 기본 도형(큐브/원기둥/구)으로 절차적으로 조립됩니다. 그래서 델타포스 수준의 실사 모델과 애니메이션은 아닙니다. 스켈레탈 메쉬(예: Game Animation Sample, MetaHuman)와 Megascans를 넣으면 크게 좋아집니다. 위의 `MO_` 오버라이드를 참고하세요.
- **멀티플레이어**: 현재는 싱글플레이(아군·적·라이벌 모두 AI)입니다.
- **차량**: Warfare 차량(전차/헬기)은 아직 없습니다. 캐릭터에 연결 지점(`CurrentVehicle`)만 마련해 두었습니다.
- **컴파일 검증**: 이 작업 환경에는 Unreal Engine이 없습니다. 대신 UE API 스텁 헤더로 모든 `.cpp`를 clang 구문·타입 검사했습니다(Unity Build, 섀도잉 경고 포함). 실제 엔진 버전에 따라 일부 API 이름이 달라 빌드 오류가 날 수 있습니다. 오류 로그를 알려주시면 바로 수정하겠습니다.
- **원본 Unity 게임**: 코드를 받으면 무기 수치, 적 행동, UI 흐름을 원본과 1:1로 맞출 수 있습니다.

> 델타포스(Delta Force)의 상표와 에셋은 원저작권자에게 있습니다. 이 프로젝트는 그 게임성과 분위기를 참고한 독립 프로젝트입니다. 맵, 오퍼레이터, 진영, 아이템 이름은 모두 새로 지었으며 원작 에셋은 사용하지 않습니다.
