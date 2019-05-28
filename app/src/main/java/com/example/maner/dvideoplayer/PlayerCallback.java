package com.example.maner.dvideoplayer;

import android.os.Handler;
import android.util.Log;

public class PlayerCallback {

    private Handler mHandler;
    PlayVideo mPlayVideo;

    public PlayerCallback(PlayVideo playVideo) {
        mPlayVideo = playVideo;
        mHandler = new Handler();
    }

    public void onAlert(final int pValue) {
        mHandler.post(new Runnable() {
            public void run() {

            }
        });
    }

    public void updateClock(double clock){

        final double fClock = clock;

        mHandler.post(new Runnable() {
            public void run() {
                Log.d("jni", "updateClock from jni: " + fClock);

                mPlayVideo.updateClock(fClock);
            }
        });

    }

    public void seekEnd(){
        mHandler.post(new Runnable() {
            public void run() {
                mPlayVideo.seekEnd();
            }
        });
    }

    public void movieEnd(){

        mHandler.post(new Runnable() {
            public void run() {

                mPlayVideo.movieEnd();
            }
        });

    }
}