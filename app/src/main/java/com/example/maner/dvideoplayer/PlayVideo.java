package com.example.maner.dvideoplayer;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Bundle;
import android.support.design.widget.FloatingActionButton;
import android.support.design.widget.Snackbar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

public class PlayVideo extends AppCompatActivity implements SurfaceHolder.Callback {

    String fileName = "";

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

        SurfaceView surfaceView = (SurfaceView) findViewById(R.id.surfaceView);
        SurfaceHolder surfaceHolder = surfaceView.getHolder();
        surfaceHolder.addCallback(this);

        //MoviePlayView playView = new MoviePlayView(this, fileName);
        //setContentView(playView);
    }

    @Override
    public void surfaceCreated(final SurfaceHolder surfaceHolderaa) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                initBasicPlayer();

                int openResult = openMovie(fileName, surfaceHolderaa.getSurface());
            }
        }).start();
    }

    public void mOnClick(View v) {
        switch (v.getId()) {
            case R.id.left:
                StreamSeek(-10);
                break;
            case R.id.right:
                StreamSeek(10);
                break;
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder surfaceHolder, int i, int i1, int i2) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder surfaceHolder) {

    }

    public static native int initBasicPlayer();
    public static native int openMovie(String filePath, Object surface);
    public static native int renderFrame(Bitmap bitmap);
    public static native int getMovieWidth();
    public static native int getMovieHeight();
    public static native void closeMovie();
    public static native void StreamSeek(double incr);
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
