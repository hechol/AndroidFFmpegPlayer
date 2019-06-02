# AndroidFFmpegPlayer

builded with Android Studio 3.3.2

===============================================================

ffmpeg library: ffmpeg-3.3.8 

ndk build: Cmake

video: android ANativeWindow

sound: OpenSL

================================================================

개발에 가장 많이 참고했던 코드는 http://dranger.com/ffmpeg/ 의 tutorial과 ffmpeg-0.5.15의 ffplay.c 코드 입니다.

ffmpeg library build는 이곳을 참고했습니다.

http://dev2.prompt.co.kr/77

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

## 성능 관련 개선이 필요한 부분

ffmpeg 라이브러리 빌드를 할때 디버깅을 위한 셋팅으로 빌드 했던걸로 기억합니다. 

https://d2.naver.com/helloworld/8794 에 소개된 shader를 사용한 최적화를 적용하면 더 높은 프레임이 나올 것입니다.

프레임 스킵 기능이 적용되어 있긴 한데 디코딩 이후의 과정(avcodec_receive_frame 이후의 부분)만 스킵이 됩니다.  
현재 디코딩속도가 재생속도보다 낮게 나오는 경우 사운드만 재생되고 화면은 멈춰있는 현상이 발생합니다. 

==========================================================

mail: maner9@gmail.com, maner9@hanmail.net
