package com.capstone.whereigo;

import android.Manifest;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.speech.SpeechRecognizer;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.animation.Animation;
import android.view.animation.LinearInterpolator;
import android.view.animation.TranslateAnimation;
import android.view.inputmethod.EditorInfo;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.constraintlayout.widget.ConstraintSet;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.preference.PreferenceManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.transition.AutoTransition;
import androidx.transition.Transition;
import androidx.transition.TransitionManager;

import com.capstone.whereigo.databinding.ActivitySearchBinding;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Objects;

public class SearchActivity extends AppCompatActivity {
    private final String TAG = "SearchActivity";
    private ActivitySearchBinding binding;
    private static final int RECORD_AUDIO_REQUEST_CODE = 100;
    private long backPressedTime = 0;
    private final long backPressInterval = 1000;
    private List<String> allResults;

    private SharedPreferences sharedPreferences;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivitySearchBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        binding.searchBarArrive.inflateMenu(R.menu.search_menu);
        binding.searchBarArrive.getMenu().findItem(R.id.action_voice_search).setOnMenuItemClickListener(item -> {
            if (checkAudioPermission()) {
                if (SpeechRecognizer.isRecognitionAvailable(this)) {
                    VoiceRecordDialog dialog = new VoiceRecordDialog();
                    dialog.setVoiceResultListener(new VoiceRecordDialog.VoiceResultListener() {
                        @Override
                        public void onVoiceResult(String command, String context) {
                            runOnUiThread(() -> {
                                if(Objects.equals(command, "navigate")){
                                    startNavigationTransition(context, 1);
                                } else if (Objects.equals(command, "settings")) {
                                    if(Objects.equals(context, "vibrate")){
                                        SharedPreferences sharedPreferences = PreferenceManager.getDefaultSharedPreferences(SearchActivity.this);
                                        boolean currentVibrate = sharedPreferences.getBoolean("vibrate", true);
                                        boolean newVibrate = !currentVibrate;
                                        sharedPreferences.edit().putBoolean("vibrate", newVibrate).apply();

                                        if (newVibrate) {
                                            Vibrator vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
                                            if (vibrator != null && vibrator.hasVibrator()) {
                                                if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
                                                    vibrator.vibrate(VibrationEffect.createOneShot(300, VibrationEffect.DEFAULT_AMPLITUDE));
                                                } else {
                                                    vibrator.vibrate(300);
                                                }
                                            }
                                        }
                                    } else if (command.equals("sdcard")) {
                                        //여기에 sdcard 토글 부분 마저 구현하기
                                        Toast.makeText(SearchActivity.this, "sdcard 설정 변경", Toast.LENGTH_SHORT).show();
                                    }
                                }
                            });
                        }
                    });
                    dialog.show(getSupportFragmentManager(), "VoiceRecordDialog");
                } else {
                    Toast.makeText(this, "음성 인식을 사용할 수 없습니다.", Toast.LENGTH_SHORT).show();
                }
            } else {
                requestAudioPermission();
            }
            return true;
        });

        binding.searchView.setupWithSearchBar(binding.searchBarArrive);

        binding.searchBarDeparture.inflateMenu(R.menu.search_menu);
        binding.searchBarDeparture.getMenu().findItem(R.id.action_voice_search).setOnMenuItemClickListener(item -> {
            if (checkAudioPermission()) {
                if (SpeechRecognizer.isRecognitionAvailable(this)) {
                    VoiceRecordDialog dialog = new VoiceRecordDialog();
                    dialog.show(getSupportFragmentManager(), "VoiceRecordDialog");
                } else {
                    Toast.makeText(this, "음성 인식을 사용할 수 없습니다.", Toast.LENGTH_SHORT).show();
                }
            } else {
                requestAudioPermission();
            }
            return true;
        });

        allResults = new ArrayList<>();
        File buildingInfo = new File(getExternalFilesDir(null), "building_info.txt");

        if (buildingInfo.exists()) {
            // 이미 파일이 있으면 바로 로드
            loadBuildingInfo(buildingInfo);
        } else {
            // 없으면 다운로드하고, 완료 후 로드
            FileDownloader.downloadBuildingInfoFile(
                    this,
                    "https://media-server-jubin.s3.amazonaws.com/building_info.txt",
                    new FileDownloader.OnUnzipCompleteListener() {
                        @Override
                        public void onComplete() {
                            File file = new File(getExternalFilesDir(null), "building_info.txt");
                            loadBuildingInfo(file);
                        }

                        @Override
                        public void onFailure(String errorMessage) {
                            Toast.makeText(SearchActivity.this, "building_info.txt 다운로드 실패", Toast.LENGTH_SHORT).show();
                        }
                    }
            );
        }

        binding.searchView.getEditText().addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
            @Override public void afterTextChanged(Editable s) {}
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                String query = s.toString().trim();
                List<String> filtered = new ArrayList<>();
                for (String item : allResults) {
                    if (item.toLowerCase().contains(query.toLowerCase())) {
                        filtered.add(item);
                    }
                }

                if (!query.isEmpty()) {
                    binding.searchResult.setVisibility(View.VISIBLE);
                    binding.searchResult.setLayoutManager(new LinearLayoutManager(SearchActivity.this));
                    binding.searchResult.setAdapter(new SearchResultAdapter(filtered, roomName -> {

//                        String buildingName = selected.split(" ")[0];
//                        String roomNumber = selected.replaceAll("[^0-9]", "");

//                        int currentFloor = 6;

//                        String fullSelected = buildingName + " " + roomNumber;

                        startNavigationTransition(roomName, 1);
                    }));
                } else {
                    binding.searchResult.setVisibility(View.GONE);
                }
            }
        });

        binding.settingsButton.setOnClickListener(v -> {
            Intent intent = new Intent(SearchActivity.this, SettingActivity.class);
            startActivity(intent);
        });

        getOnBackPressedDispatcher().addCallback(this, new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {
                if (binding.searchView.isShowing()) {
                    binding.searchView.hide();
                } else {
                    long currentTime = System.currentTimeMillis();
                    if (currentTime - backPressedTime < backPressInterval) {
                        setEnabled(false);
                        getOnBackPressedDispatcher().onBackPressed();
                    } else {
                        backPressedTime = currentTime;
                        Toast.makeText(SearchActivity.this, "뒤로 버튼을 한 번 더 누르면 종료됩니다.", Toast.LENGTH_SHORT).show();
                    }
                }
            }
        });
    }

    private void startNavigationTransition(String roomName, int currentFloor) {
        binding.searchView.hide();

        binding.searchBarDeparture.setVisibility(View.VISIBLE);
        binding.settingsButton.setVisibility(View.GONE);

        final ConstraintSet constraintSet = new ConstraintSet();
        constraintSet.clone(binding.searchMain);
        constraintSet.clear(binding.searchBarArrive.getId(), ConstraintSet.TOP);
        constraintSet.connect(binding.searchBarArrive.getId(), ConstraintSet.TOP,
                binding.searchBarDeparture.getId(), ConstraintSet.BOTTOM, 10);

        Transition transition = new AutoTransition();
        transition.setDuration(300);
        transition.addListener(new Transition.TransitionListener() {
            @Override
            public void onTransitionStart(Transition transition) {
                // 시작 시
            }

            @Override
            public void onTransitionEnd(Transition transition) {
                getSupportFragmentManager().beginTransaction()
                        .replace(R.id.path_navigation, new HelloArFragment("본부관", roomName, 1))
                        .commit();

                binding.loading.setVisibility(View.VISIBLE);
                binding.searchBarDeparture.setVisibility(View.INVISIBLE);
                binding.searchBarArrive.setVisibility(View.INVISIBLE);

                View splash = getLayoutInflater().inflate(R.layout.activity_splash, binding.loading, false);
                ImageView splashIcon = splash.findViewById(R.id.splash_icon);
                DisplayMetrics displayMetrics = new DisplayMetrics();
                getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);
                int screenWidth = displayMetrics.widthPixels;

                int moveX = (int) (screenWidth * 0.2f);

                ObjectAnimator animator = ObjectAnimator.ofFloat(splashIcon, "translationX", -moveX, moveX);
                animator.setDuration(2000);
                animator.setRepeatCount(ValueAnimator.INFINITE);
                animator.setRepeatMode(ValueAnimator.RESTART);
                animator.start();

                TextView tvSplashText = splash.findViewById(R.id.splash_text);
                tvSplashText.setText("WIGO가 경로 안내를 준비 중...");

                binding.loading.addView(splash);

                splash.postDelayed(() -> {
                    animator.cancel();
                    splash.setVisibility(View.GONE);
                    binding.searchBarDeparture.setVisibility(View.VISIBLE);
                    binding.searchBarArrive.setVisibility(View.VISIBLE);
                }, 3000);
            }

            @Override
            public void onTransitionCancel(Transition transition) {}
            @Override
            public void onTransitionPause(Transition transition) {}
            @Override
            public void onTransitionResume(Transition transition) {}
        });
        TransitionManager.beginDelayedTransition(binding.searchMain, transition);
        constraintSet.applyTo(binding.searchMain);
    }


    public String readFileToString(File file) {
        StringBuilder stringBuilder = new StringBuilder();

        try (BufferedReader reader = new BufferedReader(new FileReader(file))) {
            String line;
            while ((line = reader.readLine()) != null) {
                stringBuilder.append(line).append("\n");  // 각 줄을 StringBuilder에 추가
            }
        } catch (IOException e) {
            e.printStackTrace();
        }

        return stringBuilder.toString();  // String으로 반환
    }

    private boolean checkAudioPermission() {
        return ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
                == PackageManager.PERMISSION_GRANTED;
    }

    private void requestAudioPermission() {
        ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.RECORD_AUDIO}, RECORD_AUDIO_REQUEST_CODE);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);

        if (requestCode == RECORD_AUDIO_REQUEST_CODE) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                Toast.makeText(this, "음성 권한이 허용되었습니다.", Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(this, "음성 권한이 거부되었습니다.", Toast.LENGTH_SHORT).show();
            }
        }
    }

    private void loadBuildingInfo(File file) {
        try (BufferedReader reader = new BufferedReader(new FileReader(file))) {
            String line;
            while ((line = reader.readLine()) != null) {
                allResults.add(line);
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

}
