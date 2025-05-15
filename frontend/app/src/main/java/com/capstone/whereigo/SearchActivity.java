package com.capstone.whereigo;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.speech.SpeechRecognizer;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.inputmethod.EditorInfo;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.LinearLayoutManager;

import com.capstone.whereigo.databinding.ActivitySearchBinding;
import com.google.android.material.search.SearchBar;
import com.google.android.material.search.SearchView;

import java.util.ArrayList;
import java.util.List;

public class SearchActivity extends AppCompatActivity {

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

        SearchBar searchBar = binding.searchBar;
        SearchView searchView = binding.searchView;
        searchView.setupWithSearchBar(searchBar);
        searchBar.inflateMenu(R.menu.search_menu);

        // 음성 검색 메뉴 처리
        searchBar.getMenu().findItem(R.id.action_voice_search).setOnMenuItemClickListener(item -> {
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
        allResults.add("미래관 445호");
        allResults.add("미래관 447호");
        allResults.add("미래관 449호");
        allResults.add("미래관 444호");
        allResults.add("미래관 425호");
        allResults.add("미래관 415호");
        allResults.add("미래관 405호");

        // 검색 텍스트 변경 감지
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
                    binding.searchResult.setAdapter(new SearchResultAdapter(filtered, SearchActivity.this::navigateToMain));
                } else {
                    binding.searchResult.setVisibility(View.GONE);
                }
            }
        });

        // 키보드 Enter (검색) 키로 바로 전환
        binding.searchView.getEditText().setOnEditorActionListener((TextView v, int actionId, android.view.KeyEvent event) -> {
            if (actionId == EditorInfo.IME_ACTION_SEARCH || actionId == EditorInfo.IME_ACTION_DONE) {
                String query = binding.searchView.getText().toString().trim();
                if (!query.isEmpty()) {
                    navigateToMain(query);
                }
                return true;
            }
            return false;
        });

        // 뒤로가기 동작 제어
        getOnBackPressedDispatcher().addCallback(this, new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {
                if (searchView.isShowing()) {
                    searchView.hide();
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

        setupWakeWordListener();
    }

    private void navigateToMain(String query) {
        Intent intent = new Intent(SearchActivity.this, MainActivity.class);
        intent.putExtra("search_query", query);
        startActivity(intent);
        finish();
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
                Toast.makeText(this, "음성 권한이 허용되었습니다. 다시 시도해주세요.", Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(this, "음성 권한이 거부되었습니다.", Toast.LENGTH_SHORT).show();
            }
        }
    }

    private void setupWakeWordListener() {
        if (!checkAudioPermission()) {
            requestAudioPermission();
        }
    }
}
