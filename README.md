# AndroidFFmpegPlayer

builded with Android Studio 3.4.1

===============================================================

ffmpeg library: ffmpeg-3.3.9

ndk build: Cmake

video: android ANativeWindow

sound: OpenSL

===============================================================

ffmpeg build script

armeabi-v7a : https://github.com/hechol/AndroidFFmpegPlayer/blob/master/app/src/main/cpp/ffmpeg/armeabi-v7a/config_android_arm.sh

arm64-v8a : https://github.com/hechol/AndroidFFmpegPlayer/blob/master/app/src/main/cpp/ffmpeg/arm64-v8a/config_android_arm64.sh

================================================================

개발에 가장 많이 참고했던 코드는 http://dranger.com/ffmpeg/ 의 tutorial과 ffmpeg-0.5.15의 ffplay.c 코드 입니다.

ffmpeg library build는 이곳을 참고했습니다.

https://medium.com/@karthikcodes1999/cross-compiling-ffmpeg-4-0-for-android-b988326f16f2

http://gamdekong.tistory.com/106

http://blog.naver.com/PostView.nhn?blogId=just4u78&logNo=220628698165&categoryNo=0&parentCategoryNo=0&viewDate=&currentPage=1&postListTopCurrentPage=1&from=postView

OpenSL 관련 code는 이곳을 참고했습니다.

https://github.com/googlesamples/android-ndk/tree/master/native-audio
https://blog.csdn.net/Glouds/article/details/50944309 

android ANativeWindow 관련 code는 이곳을 참고했습니다.

https://blog.csdn.net/Glouds/article/details/50937266

================================================================

#### 빌드된 apk파일은 build폴더에 있습니다.

================================================================

## 주요 기능 및 특징 (Key Functions and Features)

프로그래시브 바를 이용해서 임의의 지점으로 seek 가능

You can seek to any point using a progressive bar

특정 구간 반복 기능 

interval repeat playback function

재생 완료 후 처음부터 다시 재생

Playback again after finish of playback

## 프레임 스킵 기능의 작동 방식

1단계: 현재 디코딩속도가 재생속도보다 낮은 경우(90프레임 이상 밀린 경우) 사운드는 정상 재생되고 화면은 bidirectional frame이 스킵됩니다.
2단계: 1단계 이후 현재 디코딩속도가 재생속도보다 낮은 경우(90프레임 이상 밀린 경우) 사운드는 정상 재생되고 화면은 key frame만 출력됩니다.

- 현재 디코딩속도가 재생속도보다 낮은 경우(10프레임 이상 밀린 경우) 사운드는 정상 재생되고 화면 출력은 생략됩니다.

==========================================================

mail: maner9@gmail.com, maner9@hanmail.net
