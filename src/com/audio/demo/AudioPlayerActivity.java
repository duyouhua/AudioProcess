package com.audio.demo;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.util.ArrayList;

import android.app.Activity;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.os.Bundle;
import android.os.Environment;
import android.os.Process;
import android.view.View;
import android.widget.EditText;
import android.widget.Toast;

import com.audio.jni.AudioPreprocessJni;
import com.audio.jni.AudioRecordJni;

public class AudioPlayerActivity extends Activity implements
		View.OnClickListener {
	private final static String TAG = AudioPlayerActivity.class.getSimpleName();
	private final static int LEVELS[]={-2,-4,-6,-8,-10,-12,-14,-16,-18,-20};
	private String mSaveFileName;
	private String mDenoiseFileName;
	private int sampleRate;
	private int channel;
	private AudioRecordJni mRecordJni;
	private int mDenoiseLevelIndex ;
	private EditText mDenoiseInput;
	private ArrayList<byte[]> soundArrays = new ArrayList<byte[]>();

	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		channel = 1;
		mDenoiseLevelIndex = LEVELS.length-1;
		setContentView(R.layout.main);
		findViewById(R.id.start_recoard).setOnClickListener(this);
		findViewById(R.id.pause_recoard).setOnClickListener(this);
		findViewById(R.id.stop_recoard).setOnClickListener(this);
		findViewById(R.id.denoise_recoard).setOnClickListener(this);
		findViewById(R.id.play_recoard).setOnClickListener(this);
		findViewById(R.id.play_denoise).setOnClickListener(this);
		mDenoiseInput = (EditText)findViewById(R.id.input_denoise_level);
		mSaveFileName = getBufferDir()+"/myorig_"+channel+"ch.pcm";
//		mSaveFileName = getBufferDir()+"/myorig.pcm";
		mDenoiseFileName = getBufferDir()+"/myproc_"+channel+"ch.pcm";
		sampleRate = 44100;
	}

	public void onDestroy(){
		super.onDestroy();
		if (mRecordJni != null) {
			mRecordJni.stop();
			mRecordJni.release();
			mRecordJni = null;
		}
		Process.killProcess(Process.myPid());
	}

	public String getBufferDir() {
		File dir = new File(Environment.getExternalStorageDirectory()
				.getAbsolutePath() + "/DCIM");
		if (!dir.exists()) {
			dir.mkdirs();
		}
		return dir.getAbsolutePath();
	}

	@Override
	public void onClick(View v) {
		switch (v.getId()) {
		case R.id.start_recoard:
			android.util.Log.e("javaLog", "java start record");
			if (mRecordJni == null) {
				if (channel == 1) {
					int minBufferSize = AudioRecord.getMinBufferSize(sampleRate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
					mRecordJni = new AudioRecordJni(mSaveFileName,
							sampleRate, 2, AudioRecordJni.CHANNEL_1, minBufferSize);
				} else {
					int minBufferSize = AudioRecord.getMinBufferSize(sampleRate, AudioFormat.CHANNEL_IN_STEREO, AudioFormat.ENCODING_PCM_16BIT);
					mRecordJni = new AudioRecordJni(mSaveFileName,
							sampleRate, 2, AudioRecordJni.CHANNEL_2, minBufferSize);
				}
			}
			mRecordJni.setOnFrameCallback(new AudioRecordJni.OnFrameCallback() {
				@Override
				public void onFrameCallbackBuffer(byte[] data, int size) {
					synchronized (soundArrays) {
						soundArrays.add(data);
						soundArrays.notify();
					}
				}
			});
			mRecordJni.start();
			Thread t = new Thread(){
				public void run(){
					String readFileName = getBufferDir()+"/myread_"+channel+"ch.pcm";
					try{
						FileOutputStream fos = new FileOutputStream(readFileName);
						while(mRecordJni!=null){
							synchronized (soundArrays) {
								if(soundArrays.size()>0){
									byte []buf = soundArrays.remove(0);
									if(buf!=null){
										fos.write(buf);
									}
								}else{
									soundArrays.wait();
								}
							}
						}
						fos.close();
					}catch(Exception e){
						e.printStackTrace();
					}
				}
			};
			t.start();
			break;
		case R.id.pause_recoard:
			if (mRecordJni != null) {
				mRecordJni.pause();
			}
			break;
		case R.id.stop_recoard:
			if (mRecordJni != null) {
				mRecordJni.stop();
				mRecordJni.release();
				mRecordJni = null;
			}
			synchronized (soundArrays) {
				soundArrays.notify();
			}
			break;
		case R.id.denoise_recoard:
			Thread dt = new Thread(){
				public void run(){
					String inputStr = mDenoiseInput.getText().toString().trim();
					if(inputStr!=null && inputStr.length()>0){
						try{
							int idx = Integer.parseInt(inputStr);
							if(idx<LEVELS.length){
								mDenoiseLevelIndex = idx;
							}
						}catch(NumberFormatException e){
							e.printStackTrace();
						}
					}
//					int minBufferSize = 0;
//					if (channel == 1) {
//						minBufferSize = AudioRecord.getMinBufferSize(sampleRate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
//					} else {
//						minBufferSize = AudioRecord.getMinBufferSize(sampleRate, AudioFormat.CHANNEL_IN_STEREO, AudioFormat.ENCODING_PCM_16BIT);
//					}
//					for(int i=-50;i<-1;i++){
//					int i = -1;
//						mDenoiseFileName = getBufferDir()+"/myproc_"+i+".pcm";
						AudioPreprocessJni.preprocess(mSaveFileName, mDenoiseFileName, sampleRate, 2, channel, LEVELS[mDenoiseLevelIndex]);
//					}
					runOnUiThread(new Runnable() {
						
						@Override
						public void run() {
							Toast.makeText(AudioPlayerActivity.this, "denoise finish", Toast.LENGTH_LONG).show();
						}
					});
				}
			};
			dt.start();
			break;
		case R.id.play_recoard:
			MyAudioTrack track = null;
			if (channel == 1) {
				track = new MyAudioTrack(sampleRate,
						AudioFormat.CHANNEL_OUT_MONO,
						AudioFormat.ENCODING_PCM_16BIT, mSaveFileName);
			} else {
				track = new MyAudioTrack(sampleRate,
						AudioFormat.CHANNEL_OUT_STEREO,
						AudioFormat.ENCODING_PCM_16BIT, mSaveFileName);
			}

			track.play();
			break;
		case R.id.play_denoise:
			MyAudioTrack trackNenoise = null;
			if (channel == 1) {
				trackNenoise = new MyAudioTrack(sampleRate,
						AudioFormat.CHANNEL_OUT_MONO,
						AudioFormat.ENCODING_PCM_16BIT, mDenoiseFileName);
			} else {
				trackNenoise = new MyAudioTrack(sampleRate,
						AudioFormat.CHANNEL_OUT_STEREO,
						AudioFormat.ENCODING_PCM_16BIT, mDenoiseFileName);
			}

			trackNenoise.play();
			break;
		}
	}
}