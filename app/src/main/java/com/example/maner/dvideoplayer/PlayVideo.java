package com.example.maner.dvideoplayer;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Bundle;
import android.os.Handler;
import android.support.design.widget.FloatingActionButton;
import android.support.design.widget.Snackbar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

public class PlayVideo extends Activity implements
        SurfaceHolder.Callback,
        GestureDetector.OnGestureListener,
        GestureDetector.OnDoubleTapListener {

    String fileName = "";

    Handler mHandler;
    SurfaceView mPreview;
    LinearLayout playerUI;;

    GestureDetector mDetector;

    Button pause;
    Button auto_repeat;

    boolean isPause = false;

    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_play_video);

        // Example of a call to a native method
        //TextView tv = (TextView) findViewById(R.id.sample_text);
        //tv.setText(stringFromJNI());

        Intent intent = getIntent();
        fileName = intent.getStringExtra("file_name");

        mPreview = (SurfaceView) findViewById(R.id.surfaceView);
        SurfaceHolder surfaceHolder = mPreview.getHolder();
        surfaceHolder.addCallback(this);

        playerUI = (LinearLayout) findViewById(R.id.player_ui);
        playerUI.setVisibility(View.GONE);

        auto_repeat = (Button) findViewById(R.id.auto_repeat);
        pause = (Button) findViewById(R.id.pause);

        mHandler = new Handler();

        mDetector = new GestureDetector(this, this);
        mDetector.setIsLongpressEnabled(false);
    }

    @Override
    public void surfaceCreated(final SurfaceHolder surfaceHolderaa) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                initBasicPlayer(surfaceHolderaa.getSurface());
                int openResult = openMovie(fileName);
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        checkRatio();
                    }
                });
            }
        }).start();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        System.out.println("onTouchEvent motionEvent: " + event.toString());

        if (mDetector.onTouchEvent(event)) {
            return true;
        }
        return super.onTouchEvent(event);
    }

    final int auto_repeat_off= 0;
    final int auto_repeat_select_A = 1;
    final int auto_repeat_on = 2;
    int auto_repeat_state = auto_repeat_off;

    public void mOnClick(View v) {
        switch (v.getId()) {
            case R.id.close:
                playerUI.setVisibility(View.GONE);
                break;
            case R.id.left:
                StreamSeek(-10);
                break;
            case R.id.right:
                StreamSeek(10);
                break;
            case R.id.pause:
                clickPause();
                if(!isPause) {
                    isPause = true;
                    pause.setText("play");
                }else{
                    isPause = false;
                    pause.setText("pause");
                }
                break;
            case R.id.auto_repeat:
                if(auto_repeat_state == auto_repeat_off){
                    auto_repeat_state = auto_repeat_select_A;
                    auto_repeat.setText("auto_repeat_A");
                    changeAutoRepeatState(auto_repeat_select_A);
                }else if(auto_repeat_state == auto_repeat_select_A){
                    if(getAutoRepeatStartPosition() < getCurrentPosition()){
                        auto_repeat_state = auto_repeat_on;
                        auto_repeat.setText("auto_repeat_AB");
                        changeAutoRepeatState(auto_repeat_on);
                    }else{
                        auto_repeat_state = auto_repeat_off;
                        auto_repeat.setText("auto_repeat");
                        changeAutoRepeatState(auto_repeat_off);
                    }
                }else if(auto_repeat_state == auto_repeat_on){
                    auto_repeat_state = auto_repeat_off;
                    auto_repeat.setText("auto_repeat");
                    changeAutoRepeatState(auto_repeat_off);
                }
                break;
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder surfaceHolder, int i, int i1, int i2) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder surfaceHolder) {

    }

    private void checkRatio() {
        int surfaceView_Width = mPreview.getWidth();
        int surfaceView_Height = mPreview.getHeight();

        float video_Width = getMovieWidth();
        float video_Height = getMovieHeight();

        float ratio_width = surfaceView_Width / video_Width;
        float ratio_height = surfaceView_Height / video_Height;
        float aspectratio = video_Width / video_Height;

        ViewGroup.LayoutParams layoutParams = mPreview.getLayoutParams();
        if (ratio_width > ratio_height) {
            layoutParams.width = (int) (surfaceView_Height * aspectratio);
            layoutParams.height = surfaceView_Height;
        } else {
            layoutParams.width = surfaceView_Width;
            layoutParams.height = (int) (surfaceView_Width / aspectratio);
        }
        mPreview.setLayoutParams(layoutParams);
    }

    @Override
    public void onBackPressed() {
        close();
    }

    @Override
    protected void onDestroy()
    {
        super.onDestroy();
    }

    @Override
    public boolean onSingleTapConfirmed(MotionEvent motionEvent) {
        System.out.println("onSingleTapConfirmed motionEvent: " + motionEvent.getY());
        return false;
    }

    @Override
    public boolean onDoubleTap(MotionEvent motionEvent) {
        System.out.println("onDoubleTap motionEvent: " + motionEvent.getY());
        return false;
    }

    @Override
    public boolean onDoubleTapEvent(MotionEvent motionEvent) {
        System.out.println("onDoubleTapEvent motionEvent: " + motionEvent.getY());
        return false;
    }

    @Override
    public boolean onDown(MotionEvent motionEvent) {
        System.out.println("onDown motionEvent: " + motionEvent.getY());
        return false;
    }

    @Override
    public void onShowPress(MotionEvent motionEvent) {
        System.out.println("onShowPress motionEvent: " + motionEvent.getY());
    }

    @Override
    public boolean onSingleTapUp(MotionEvent motionEvent) {
        System.out.println("onSingleTapUp motionEvent: " + motionEvent.getY());

        playerUI.setVisibility(View.VISIBLE);
        return false;
    }

    @Override
    public boolean onScroll(MotionEvent motionEvent, MotionEvent motionEvent1, float v, float v1) {
        System.out.println("onScroll motionEvent: " + motionEvent.getY());
        System.out.println("onScroll motionEvent1: " + motionEvent1.getY());
        System.out.println("onScroll v: " + v);
        System.out.println("onScroll v1: " + v1);

        return false;
    }

    @Override
    public void onLongPress(MotionEvent motionEvent) {
        System.out.println("onLongPress motionEvent: " + motionEvent.getY());
    }

    @Override
    public boolean onFling(MotionEvent motionEvent, MotionEvent motionEvent1, float v, float v1) {
        System.out.println("onFling motionEvent: " + motionEvent.getY());
        System.out.println("onFling motionEvent1: " + motionEvent1.getY());
        System.out.println("onFling v: " + v);
        System.out.println("onFling v1: " + v1);

        // gestureVolumeLabel.setVisibility(View.GONE);

        return false;
    }

    public static native int initBasicPlayer(Object surface);
    public static native int openMovie(String filePath);
    public static native int getMovieWidth();
    public static native int getMovieHeight();
    public static native void close();
    public static native void StreamSeek(double incr);

    public static native void changeAutoRepeatState(int state);
    public static native double getAutoRepeatStartPosition();
    public static native double getCurrentPosition();
    public static native void clickPause();
}



/*
class MoviePlayView extends View {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    private Bitmap mBitmap;

    public MoviePlayView(Context context, String filename) {
        super(context);

        if (initBasicPlayer() < 0) {
            Toast.makeText(context, "CPU doesn't support NEON", Toast.LENGTH_LONG).show();

            ((Activity)context).finish();
        }

        initBasicPlayer();

        int openResult = openMovie(filename);
        if (openResult < 0) {
            Toast.makeText(context, "Open Movie Error: " + openResult, Toast.LENGTH_LONG).show();

            ((Activity)context).finish();
        }
        else
            mBitmap = Bitmap.createBitmap(getMovieWidth(), getMovieHeight(), Bitmap.Config.RGB_565);
    }

    boolean isReady = true;
    @Override
    protected void onDraw(Canvas canvas) {
        if(isReady == true) {
            isReady = true;
            //renderFrame(mBitmap);
            //canvas.drawBitmap(mBitmap, 0, 0, null);

            invalidate();
            //isReady = true;
        }
    }


    public static native int initBasicPlayer();
    public static native int openMovie(String filePath);
    public static native int renderFrame(Bitmap bitmap);
    public static native int getMovieWidth();
    public static native int getMovieHeight();
    public static native void closeMovie();
}
*/
