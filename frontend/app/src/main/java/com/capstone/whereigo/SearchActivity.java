package com.capstone.whereigo;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.speech.SpeechRecognizer;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.inputmethod.EditorInfo;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.constraintlayout.widget.ConstraintSet;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
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

public class SearchActivity extends AppCompatActivity {
    private final String TAG = "SearchActivity";
    private ActivitySearchBinding binding;
    private static final int RECORD_AUDIO_REQUEST_CODE = 100;
    private long backPressedTime = 0;
    private final long backPressInterval = 1000;
    private List<String> allResults;

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
        File[] folders = getExternalFilesDir(null).listFiles();
        if (folders != null) {
            for (File folder : folders) {
                if (folder.isDirectory()) {
                    File file = new File(folder, "label.txt");

                    if (!file.exists()) {
                        continue;
                    }

                    String jsonString = readFileToString(file);
                    if (jsonString == null || jsonString.trim().isEmpty()) {
                        android.util.Log.w(TAG, "label.txt is empty in folder: " + folder.getName());
                        continue;
                    }

                    try {
                        JSONObject jsonObject = new JSONObject(jsonString);
                        Iterator<String> keys = jsonObject.keys();

                        while (keys.hasNext()) {
                            allResults.add(folder.getName() + " " + keys.next());
                        }
                    } catch (JSONException e) {
                        android.util.Log.e(TAG, "Invalid JSON in folder: " + folder.getName(), e);
                        continue;
                    }
                }
            }
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
                    binding.searchResult.setAdapter(new SearchResultAdapter(filtered, selected -> {

                        String buildingName = selected.split(" ")[0];
                        String roomNumber = selected.replaceAll("[^0-9]", "");

                        int currentFloor = 6;

                        String fullSelected = buildingName + " " + roomNumber;

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
                        TransitionManager.beginDelayedTransition(binding.searchMain, transition);

                        constraintSet.applyTo(binding.searchMain);

                        getSupportFragmentManager().beginTransaction()
                                .replace(R.id.path_navigation, new HelloArFragment(fullSelected, currentFloor))
                                .commit();
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
}
