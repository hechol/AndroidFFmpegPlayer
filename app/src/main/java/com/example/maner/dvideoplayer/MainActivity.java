package com.example.maner.dvideoplayer;

import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Environment;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class MainActivity extends AppCompatActivity {

    String mCurrent;
    String mRoot;
    TextView mCurrentTxt;
    ListView mFileList;
    ArrayAdapter<String> mAdapter;
    ArrayList<String> arFiles;

    List<String> permissions;

    static {
        System.loadLibrary("native-lib");
    }

    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        permissions = new ArrayList<String>();
        permissions.add(android.Manifest.permission.READ_EXTERNAL_STORAGE);

        mCurrentTxt = (TextView)findViewById(R.id.current);
        mFileList = (ListView)findViewById(R.id.filelist);

        arFiles = new ArrayList<String>();
        mRoot = Environment.getExternalStorageDirectory().getAbsolutePath();
        mCurrent = mRoot;

        mAdapter = new ArrayAdapter<String>(this,
                android.R.layout.simple_list_item_1, arFiles);
        mFileList.setAdapter(mAdapter);
        mFileList.setOnItemClickListener(mItemClickListener);

        initBasicPlayer();

        checkPermissions();
    }

    public void checkPermissions(){
        System.out.println("initAfterCreaveView start");

        if(permissions.size() < 1){
            refreshFiles();
            return;
        }

        final String permission = permissions.get(0);

        // Here, thisActivity is the current activity
        if (ContextCompat.checkSelfPermission(this,
                permission)
                != PackageManager.PERMISSION_GRANTED) {

            // Should we show an explanation?

            // No explanation needed, we can request the permission.

            ActivityCompat.requestPermissions(this,
                    new String[]{permission},
                    PERMISSION_REQUEST_CODE);

            // MY_PERMISSIONS_REQUEST_READ_CONTACTS is an
            // app-defined int constant. The callback method gets the
            // result of the request.
        }else{
            if(permissions.size() > 0){
                permissions.remove(0);
                checkPermissions();
            }
        }
    }

    final int PERMISSION_REQUEST_CODE = 0;

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           String permissionArray[], int[] grantResults) {

        final String permission = permissionArray[0];

        switch (requestCode) {
            case PERMISSION_REQUEST_CODE: {
                // If request is cancelled, the result arrays are empty.
                if (grantResults.length > 0
                        && grantResults[0] == PackageManager.PERMISSION_GRANTED) {

                    // permission was granted, yay! Do the
                    // contacts-related task you need to do.
                    if(permissions.size() > 0) {
                        permissions.remove(0);
                        checkPermissions();
                    }else{

                    }

                } else {

                    if (ActivityCompat.shouldShowRequestPermissionRationale(this,
                            permission)) {
                        new AlertDialog.Builder(this, R.style.Base_Theme_AppCompat_Dialog)
                                .setTitle("dplayer")
                                .setMessage("어플을 시작하기 위해서는 " +
                                        "권한 동의가 필요합니다. \n계속 진행하겠습니까?")
                                .setPositiveButton("예", new DialogInterface.OnClickListener() {
                                    public void onClick(DialogInterface dialog, int whichButton) {
                                        ActivityCompat.requestPermissions(MainActivity.this,
                                                new String[]{permission},
                                                PERMISSION_REQUEST_CODE);
                                    }
                                })
                                .setNegativeButton("아니오", new DialogInterface.OnClickListener() {
                                    public void onClick(DialogInterface dialog, int whichButton) {

                                    }
                                })
                                .show();
                    } else {
                        new AlertDialog.Builder(this, R.style.Base_Theme_AppCompat_Dialog)
                                .setTitle("dplayer")
                                .setMessage("어플을 시작하기 위해서는 권한 동의가 필요합니다. \n" +
                                        "설정 -> 어플리케이션에서 권한 설정을 확인해 보세요.")
                                .setPositiveButton("확인", new DialogInterface.OnClickListener() {
                                    @Override
                                    public void onClick(DialogInterface dialog, int which) {

                                    }
                                }).show();

                    }
                }
                return;
            }

            // other 'case' lines to check for other
            // permissions this app might request
        }
    }

    AdapterView.OnItemClickListener mItemClickListener =
            new AdapterView.OnItemClickListener() {
                public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                    String Name = arFiles.get(position);
                    if (Name.startsWith("[") && Name.endsWith("]")) {
                        Name = Name.substring(1, Name.length()-1);
                    }
                    String Path = mCurrent + "/" + Name;
                    File f = new File(Path);
                    if (f.isDirectory()) {
                        mCurrent = Path;
                        refreshFiles();
                    } else {

                        int openResult = openMovie(Path);
                        //Toast.makeText(this, arFiles.get(position),
                        //        Toast.LENGTH_SHORT).show();
                        if(openResult == 0) {
                            Intent tempIntent = new Intent(getApplicationContext(), PlayVideo.class);
                            tempIntent.putExtra("file_name", Path);
                            startActivity(tempIntent);
                        }else{
                            new AlertDialog.Builder(MainActivity.this)
                                    .setTitle("dplayer")
                                    .setMessage("can't play this file")
                                    .setPositiveButton("ok", null)
                                    .show();
                        }
                    }
                }
            };

    public void mOnClick(View v) {
        switch (v.getId()) {
            case R.id.btnroot:
                if (mCurrent.compareTo(mRoot) != 0) {
                    mCurrent = mRoot;
                    refreshFiles();
                }
                break;
            case R.id.btnup:
                if (mCurrent.compareTo(mRoot) != 0) {
                    int end = mCurrent.lastIndexOf("/");
                    String uppath = mCurrent.substring(0, end);
                    mCurrent = uppath;
                    refreshFiles();
                }
                break;
        }
    }

    void refreshFiles() {
        mCurrentTxt.setText(mCurrent);
        arFiles.clear();
        File current = new File(mCurrent);
        String[] files = current.list();
        if (files != null) {
            for (int i = 0; i < files.length;i++) {
                String Path = mCurrent + "/" + files[i];
                String Name = "";
                File f = new File(Path);
                if (f.isDirectory()) {
                    Name = "[" + files[i] + "]";
                } else {
                    Name = files[i];
                }

                if (f.isDirectory()) {
                    boolean haveVedioFile = false;
                    String[] filesInDirectory = f.list();
                    if (filesInDirectory != null) {
                        for (int index = 0; index < filesInDirectory.length; index++) {
                            if(haveVedioFile == false) {
                                haveVedioFile = filesInDirectory[index].contains(".mp4");
                            }
                            if(haveVedioFile == false) {
                                haveVedioFile = filesInDirectory[index].contains(".avi");
                            }
                            if(haveVedioFile == false) {
                                haveVedioFile = filesInDirectory[index].contains(".wmv");
                            }
                        }
                    }

                    if (haveVedioFile) {
                        arFiles.add(Name);
                    }
                }else{
                    arFiles.add(Name);
                }
            }
        }
        mAdapter.notifyDataSetChanged();
    }

    @Override
    public void onBackPressed() {
        if (mCurrent.compareTo(mRoot) == 0) {
            onBackPressed();
        }else{
            int end = mCurrent.lastIndexOf("/");
            String uppath = mCurrent.substring(0, end);
            mCurrent = uppath;
            refreshFiles();
        }
    }

    public static native int initBasicPlayer();
    public static native int openMovie(String filePath);

}

