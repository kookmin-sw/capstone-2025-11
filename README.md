[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/gFPznrUY)
# WIGO - 시각 장애인 및 초행자용 실내 네비게이션 

## 1.1. 프로젝트 명

![베너](https://github.com/user-attachments/assets/c1749dba-18ca-45a9-9aa0-a7bd65ca3682)

## 1.2. 프로젝트 기간
* 프로젝트 기간 : (2025.01.03 ~ 2025.05.30)
<br>(추후 사진 추가)

## 1.3. 프로젝트 팀 소개
| 이강욱  | 박주빈 | 박재영 | 안선영 | 조성준 |
| --- | --- | --- | --- | --- |
| ![image](https://github.com/user-attachments/assets/a792a80a-c00a-4620-9855-04c30a36f8a4) |  ![image](https://github.com/user-attachments/assets/0825338d-3674-4d68-a075-d780631c0ea6) | ![image](https://github.com/user-attachments/assets/6939ad93-3f6f-43d1-8754-46ae9612e22b) | ![image](https://github.com/user-attachments/assets/773aed81-3c3c-41de-a940-a0d2ffaec6b2) | ![image](https://github.com/user-attachments/assets/f23b5fcd-a55e-453f-a622-8eee4c79f429) |
| Project Leader, Back-End (SLAM 및 Android-SLAM 간 통신 인터페이스 구축 담당) | Back-End (SLAM 및 미디어 서버 구축) | Full Stack (Android UI/UX 및 미디어 서버 구축) | Back-End (SLAM 및 미디어 서버 구축) | Front End (Android UI/UX) 및 Android-SLAM 간 통신 인터페이스 구축 담당 |

## 1.4. 프로젝트 소개 영상
(추후 업로드)

## 2.1. 프로젝트 개요
![캡스톤디자인_포스터_1차_시안_최종 (2)_250518_182049](https://github.com/user-attachments/assets/f5471e33-b4d0-4d97-89a0-abd4ed261ae4)
### 작성한 논문 초록
<img src="https://github.com/user-attachments/assets/33537586-9c77-4079-ab55-e915b4463773" width="500px" />
<img src="https://github.com/user-attachments/assets/2bb07bba-86dc-4898-aa32-641098d77859" width="500px" />

이 프로젝트는 시각장애인, 초행자, 노약자를 위한 실내 내비게이션 시스템을 개발하는 것을 목표로 합니다. 일반적인 내비게이션이 주로 야외에서 활용되는 반면, 본 프로젝트는 실내 환경에서도 목적지까지 안전하게 이동할 수 있도록 지원합니다.

### 📍 주요 특징
 -  Visual SLAM 기반 실내 위치 인식
 - 스마트폰 카메라 및 센서 활용 길 안내
 - 음성 안내 & 진동 피드백을 통한 직관적 인터페이스
 - 특수 장비 없이 스마트폰만으로 사용 가능

이 시스템은 시각장애인뿐만 아니라 초행자, 노약자도 손쉽게 이용할 수 있도록 설계되었습니다. 복잡한 실내 환경에서도 안전하고 편리한 길 안내를 제공합니다.
기존 실내 내비게이션은 비싼 장비나 **별도의 인프라(비콘, RFID 등)** 가 필요해 접근성이 낮다는 한계가 있습니다. 또한, 시각장애인은 실내에서 점자 블록이나 안내견에 의존하지만, 모든 공간에서 이러한 지원을 받을 수 있는 것은 아닙니다.

### 💡 이 프로젝트가 필요한 이유<br>
- 누구나 쉽게 접근 가능한 실내 내비게이션 필요
- 스마트폰 보급률 증가 → 별도 장비 없이 사용 가능
- 시각장애인, 초행자, 노약자도 독립적인 이동 가능

### 🎯 프로젝트 차별점<br>
- 고가 장비 없이 스마트폰만으로 실내 위치 인식
- 초행자도 직관적으로 사용 가능
- 공공기관, 쇼핑몰, 지하철 등 다양한 공간에서 활용 가능

## 2.2. 애플리케이션 프로세스
![image](https://github.com/user-attachments/assets/2e5356be-1deb-41a8-8055-a8772a73b471)

## 2.3. 주요 기능
- 사용자의 실내 위치를 파악하고 화면에 표시한다.
- 실내에서 사용자가 원하는 목적지를 선택하면 적절한 경로를 화면과 음성 피드백으로
안내한다.
- 경로는 주요 타겟층인 시각장애인들을 고려하여 계단 등 위험한 장소보다 엘리베이터를
사용할 수 있도록 유도하는 경로로 작성한다.
- 실시간으로 사용자의 이동 경로를 추적하여 경로 이탈 시 알림 혹은 경로 재생성을
진행한다

## 3.1. 개발환경
| 항목           | 내용                                 |
|----------------|--------------------------------------|
| 운영체제       | ![OS](https://img.shields.io/badge/Windows_11-0078D6?style=flat-square&logo=windows&logoColor=white) ![OS](https://img.shields.io/badge/Ubuntu_20.04-E95420?style=flat-square&logo=ubuntu&logoColor=white) |
| 사용 언어      | ![Kotlin](https://img.shields.io/badge/Kotlin-7F52FF?style=flat-square&logo=kotlin&logoColor=white) ![Java](https://img.shields.io/badge/Java-007396?style=flat-square&logo=java&logoColor=white) ![C++](https://img.shields.io/badge/C++-00599C?style=flat-square&logo=c%2b%2b&logoColor=white) |
| Android 버전   | ![Android](https://img.shields.io/badge/Android_13-3DDC84?style=flat-square&logo=android&logoColor=white) |
| 개발 도구      | ![Android Studio](https://img.shields.io/badge/Android_Studio-3DDC84?style=flat-square&logo=android-studio&logoColor=white) ![VS Code](https://img.shields.io/badge/VS_Code-007ACC?style=flat-square&logo=visual-studio-code&logoColor=white) |
| 버전 관리      | ![GitHub](https://img.shields.io/badge/GitHub-181717?style=flat-square&logo=github&logoColor=white) |

## 3.2. 사용한 API
- Google Voice Recognizer API
- Google Text To Speech API
- GPT 4o mini API

## 3.3. 필요한 라이브러리
<table border="1" cellspacing="0" cellpadding="6">
  <thead>
    <tr>
      <th>라이브러리</th>
      <th>설명</th>
      <th>버전</th>
      <th>비고</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>Google ARCore</td>
      <td>증강현실 기능을 위한 Google의 AR 플랫폼</td>
      <td></td>
      <td></td>
    </tr>
    <tr>
      <td>Ceres Solver</td>
      <td>비선형 최적화를 위한 라이브러리</td>
      <td></td>
      <td rowspan="6">NDK 21.4 사용<br>build_dependencies.bash 스크립트 참조</td>
    </tr>
    <tr>
      <td>Eigen3</td>
      <td>헤더 온리 선형대수 라이브러리</td>
      <td></td>
    </tr>
    <tr>
      <td>OpenCV</td>
      <td>컴퓨터 비전 및 이미지 처리 라이브러리</td>
      <td></td>
    </tr>
    <tr>
      <td>glog</td>
      <td>Google Logging 라이브러리</td>
      <td></td>
    </tr>
    <tr>
      <td>Boost</td>
      <td>범용 C++ 템플릿 라이브러리 집합</td>
      <td></td>
    </tr>
  </tbody>
</table>

##  4. 화면설계 및 기능 구현
![manual](https://github.com/user-attachments/assets/d5b2c748-e1f9-4923-9d9c-dd5013e6725b)

##  5.1. 최종 결과
### 5.1.1. 프로젝트 수행 결과물 목록 및 기술 문서 보유 여부
<table border="1" cellspacing="0" cellpadding="6">
  <thead>
    <tr>
      <th>결과물 번호</th>
      <th>결과물 명칭</th>
      <th>설명</th>
      <th>기술 문서 유무</th>
      <th>비고</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>1</td>
      <td>모바일 기반 Visual-Inertial SLAM 위치 인식 시스템</td>
      <td>ARCore 및 VINS-Mono 기반 실내 위치 추정 시스템 개발.</td>
      <td>있음</td>
      <td>부록 1 참조</td>
    </tr>
    <tr>
      <td>2</td>
      <td>Pose Graph 기반 위치 최적화 엔진</td>
      <td>누적 오차 보정을 위한 최적화 알고리즘 설계 및 구현.</td>
      <td>있음</td>
      <td>부록 1, 부록 2 모두 해당</td>
    </tr>
    <tr>
      <td>3</td>
      <td>경량화된 SLAM 프레임워크</td>
      <td>모바일 환경에서의 실시간 처리를 위한 연산 최적화 및 SLAM 시스템 경량화.</td>
      <td>있음</td>
      <td>부록 2 참조</td>
    </tr>
    <tr>
      <td>4</td>
      <td>실시간 경로 탐색 및 안내 모듈 (A 알고리즘)*</td>
      <td>목적지 설정 후 최적 경로 탐색 및 시각화 인터페이스 구현.</td>
      <td>있음</td>
      <td>부록 1 참조</td>
    </tr>
    <tr>
      <td>5</td>
      <td>음성 기반 사용자 인터페이스 (STT/TTS)</td>
      <td>실내 내비게이션 기능을 위한 음성 명령 인식 및 음성 안내 기능 개발.</td>
      <td>있음</td>
      <td>부록 1 참조</td>
    </tr>
    <tr>
      <td>6</td>
      <td>CTE 기반 성능 평가 지표 및 실험 결과 데이터</td>
      <td>Cross-Track Error 분석을 통한 정확도 평가.</td>
      <td>있음</td>
      <td>부록 1, 2 참조</td>
    </tr>
    <tr>
      <td>7</td>
      <td>Ground Truth 기반 경로 시각화 비교 데이터셋</td>
      <td>실제 측정 경로와 시스템 예측 경로 간의 비교를 위한 실험 데이터 시각화.</td>
      <td>있음</td>
      <td>부록 1, 2 모두 포함됨</td>
    </tr>
    <tr>
      <td>8</td>
      <td>실험용 지도 및 인터페이스(UI) 디자인</td>
      <td>국민대학교 미래관 4층/6층 기반 테스트 환경 설정 및 UI 요소 구성.</td>
      <td>있음</td>
      <td>부록 1 참조</td>
    </tr>
    <tr>
      <td>9</td>
      <td>자동 미분 최적화 → 수동 미분 기법 전환에 따른 연산 개선 엔진</td>
      <td>최적화 성능 향상을 위한 수학적 미분 방식 변경 및 Ceres Solver 최적화.</td>
      <td>있음</td>
      <td>부록 2 참조</td>
    </tr>
    <tr>
      <td>10</td>
      <td>사용자 맞춤형 실내 내비게이션 시나리오 및 응용 가능성 분석 보고서</td>
      <td>박물관, 쇼핑몰 등에서의 실내 길찾기 응용 방안 제시.</td>
      <td>있음</td>
      <td>부록 1, 2 결론부에 포함</td>
    </tr>
  </tbody>
</table>

### 5.1.2. 기술 문서 내용 요약
- 부록 1: Visual-Inertial SLAM 기반 실내 위치 및 환경 인식 시스템 설계에 관한 연구
    - ARCore의 Motion Tracking 모듈과 VINS-Mono의 Pose Graph Optimization 기법을 융합하여 모바일 기반 실내 내비게이션 시스템을 설계함.
    - 국민대학교 미래관 4층을 테스트 장소로 선정하여 실시간 위치 추정, 경로 탐색(A*), 음성 안내 시스템(STT, TTS) 통합 개발.
    - 실험 결과, 평균 오차 0.4191m, 음성 인식 정확도 96% 기록.
    - 별도의 인프라 없이 모바일 기기만으로 정확한 실내 길찾기가 가능하다는 점을 실증함.

- 부록 2: ARCore SDK 기반 초기 위치 추정을 위한 추론 알고리즘 설계에 관한 연구
    - ARCore 위치 데이터에 기반한 Pose Graph 구성 및 최적화를 통해 실내 위치 정밀도를 향상시킴.
    - 자동 미분을 수동 미분으로 전환하여 처리 시간 89.4% 단축, 평균 CTE 0.0958m로 기존 대비 약 69.4% 정확도 향상.
    - 국민대학교 미래관 6층에서 보행 실험 수행, 실제 위치(GT)와 예측 궤적 간의 차이를 시각화 및 분석함.
    - 모바일 디바이스에서도 고성능 SLAM 시스템을 경량화하여 실시간 운용이 가능함을 입증함.

### 5.1.3. 기술 문서 부록 구성 안내
- 부록 1: [《Visual-Inertial SLAM 기반 실내 위치 및 환경 인식 시스템 설계에 관한 연구》](https://github.com/user-attachments/files/20244542/Visual-Inertial.SLAM.pdf)
- 부록 2: [《ARCore SDK 기반 초기 위치 추정을 위한 추론 알고리즘 설계에 관한 연구》](https://github.com/user-attachments/files/20244538/ARCore.SDK.pdf)


각 문서는 해당 결과물의 기술적 근거와 구현 내용을 구체적으로 포함하고 있으며, 관련된 알고리즘, 실험 방법, 성능 평가 결과 등을 기술적으로 명세화하는 과정에서 논문으로 정리하면서 학술적 연구로 발전시킬 수 있었다.

##  5.2. 개선 해야 할 사항
(추후 작성)
