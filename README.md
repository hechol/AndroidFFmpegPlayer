# AndroidFFmpegPlayer

sound: OpenSL

video: android ANativeWindow

ndk: Cmake


ffmpeg library는 ffmpeg-0.11.5버전을 build했습니다. 


개발에 가장 많이 참고했던 코드는 
ffmpeg-0.5.15의 ffplay.c 코드 입니다.


OpenSL 관련 code는 이곳을 참고했습니다.

https://github.com/googlesamples/android-ndk/tree/master/native-audio

https://blog.csdn.net/Glouds/article/details/50944309 


android ANativeWindow 관련 code는 이곳을 참고했습니다.

https://blog.csdn.net/Glouds/article/details/50937266

apk 파일은 AndroidFFmpegPlayer/build/ 에 저장되어 있습니다. 

==========================================================

성능 관련 개선이 필요한 부분

고화질영상을 플레이 시에 기기의 cpu에 따라서 영상이 밀리는 현상이 발생합니다.

이때에도 사운드는 밀리지 않고 출력되어서 사운드와 영상의 싱크가 맞지  됩니다.

이부분은 개선할 예정입니다.

그리고 https://d2.naver.com/helloworld/8794 에 소개된 shader를 사용한 최적화를 

적용하면 더 높은 프레임이 나올 것입니다.
