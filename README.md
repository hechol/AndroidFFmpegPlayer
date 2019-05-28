# AndroidFFmpegPlayer

video: android ANativeWindow
sound: OpenSL
ndk build: Cmake


ffmpeg library는 ffmpeg-3.3.8버전을 linux환경에서 build했습니다. 


개발에 가장 많이 참고했던 코드는 
http://dranger.com/ffmpeg/ 의 tutorial과
ffmpeg-0.5.15의 ffplay.c 코드 입니다.

ffmpeg library build는 이곳을 참고했습니다.
http://dev2.prompt.co.kr/77
http://gamdekong.tistory.com/106
http://blog.naver.com/PostView.nhn?blogId=just4u78&logNo=220628698165&categoryNo=0&parentCategoryNo=0&viewDate=&currentPage=1&postListTopCurrentPage=1&from=postView


OpenSL 관련 code는 이곳을 참고했습니다.
https://github.com/googlesamples/android-ndk/tree/master/native-audio
https://blog.csdn.net/Glouds/article/details/50944309 


android ANativeWindow 관련 code는 이곳을 참고했습니다.
https://blog.csdn.net/Glouds/article/details/50937266
apk 파일은 AndroidFFmpegPlayer/build/ 에 저장되어 있습니다. 

==========================================================

성능 관련 개선이 필요한 부분

최적화가 덜 되어서 저사양 기기로 영상을 재생하면 재생이 원할하지 않을 수 있습니다.
프레임 스킵 기능이 적용되어 있긴 한데 디코딩 이후의 과정만 스킵이 적용되어서 이 부분을 개선하면 더 좋아질 것입니다.
그리고 https://d2.naver.com/helloworld/8794 에 소개된 shader를 사용한 최적화를 적용하면 더 높은 프레임이 나올 것입니다.

==========================================================

mail: maner9@gmail.com, maner9@hanmail.net
